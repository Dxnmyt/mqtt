#include "serial.h"

extern UART_HandleTypeDef huart2; // For printf redirection

static UART_HandleTypeDef* p_huart_esp = NULL;
static uint8_t esp_single_byte_rx_buffer[1];

#define ESP_UART_RX_BUFFER_SIZE 256
static uint8_t esp_uart_rx_buffer_internal[ESP_UART_RX_BUFFER_SIZE];
static volatile uint16_t esp_uart_rx_write_idx_internal = 0;
static volatile uint16_t esp_uart_rx_read_idx_internal  = 0;

#define ESP_UART_DEFAULT_TX_TIMEOUT 1000

// ===== 基础UART函数 =====

HAL_StatusTypeDef ESP_UART_Init(UART_HandleTypeDef *huart_handle) {
    if (huart_handle == NULL) {
        return HAL_ERROR;
    }
    p_huart_esp = huart_handle;
    esp_uart_rx_write_idx_internal = 0;
    esp_uart_rx_read_idx_internal = 0;
    memset(esp_uart_rx_buffer_internal, 0, ESP_UART_RX_BUFFER_SIZE);
    return HAL_UART_Receive_IT(p_huart_esp, esp_single_byte_rx_buffer, 1);
}

void ESP_UART_SendString(const char* cmd) {
    if(p_huart_esp != NULL && cmd != NULL) {
        char full_buf[256];
        int len = snprintf(full_buf, sizeof(full_buf), "%s\r\n", cmd);
        if(len > 0 && len < sizeof(full_buf)) {
            HAL_UART_Transmit(p_huart_esp, (uint8_t *)full_buf, len, ESP_UART_DEFAULT_TX_TIMEOUT);
        }
    }
}

void ESP_UART_SendBytes(const uint8_t* data, uint16_t len) {
    if(p_huart_esp != NULL && data != NULL && len > 0) {
        HAL_UART_Transmit(p_huart_esp, (uint8_t *)data, len, ESP_UART_DEFAULT_TX_TIMEOUT);
    }
}

int16_t ESP_UART_ReadByte(void) {
    if(esp_uart_rx_write_idx_internal != esp_uart_rx_read_idx_internal) {
        uint8_t byte = esp_uart_rx_buffer_internal[esp_uart_rx_read_idx_internal];
        esp_uart_rx_read_idx_internal = (esp_uart_rx_read_idx_internal + 1) % ESP_UART_RX_BUFFER_SIZE;
        return byte;
    }
    return -1;
}

uint16_t ESP_UART_ReadLine(char* line_buffer, uint16_t buffer_capacity, uint32_t timeout_ms) {
    if(line_buffer == NULL || buffer_capacity == 0) {
        return 0;
    }

    uint16_t current_len = 0;
    uint32_t start_tick = HAL_GetTick();

    while(current_len < (buffer_capacity - 1)) {
        if((HAL_GetTick() - start_tick) >= timeout_ms) {
            break;
        }

        int16_t byte_read = ESP_UART_ReadByte();
        if(byte_read != -1) {
            line_buffer[current_len++] = (char)byte_read;
            if((char)byte_read == '\n') {
                break;
            }
        } else {
            HAL_Delay(1);
        }
    }
    line_buffer[current_len] = '\0';
    return current_len;
}

void ESP_UART_ClearRxBuffer(void) {
    esp_uart_rx_read_idx_internal = esp_uart_rx_write_idx_internal;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(p_huart_esp != NULL && huart->Instance == p_huart_esp->Instance) {
        uint16_t next_write_idx = (esp_uart_rx_write_idx_internal + 1) % ESP_UART_RX_BUFFER_SIZE;
        if(next_write_idx != esp_uart_rx_read_idx_internal) {
            esp_uart_rx_buffer_internal[esp_uart_rx_write_idx_internal] = esp_single_byte_rx_buffer[0];
            esp_uart_rx_write_idx_internal = next_write_idx;
        }
        HAL_UART_Receive_IT(p_huart_esp, esp_single_byte_rx_buffer, 1);
    }
}

int fputc(int ch, FILE *f) {
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

// ===== 辅助函数 =====

// 检查响应中是否包含特定字符串
static uint8_t check_response_contains(const char* expected_response, uint32_t timeout_ms) {
    char response_buffer[256];
    uint32_t start_time = HAL_GetTick();
    
    while ((HAL_GetTick() - start_time) < timeout_ms) {
        uint16_t len = ESP_UART_ReadLine(response_buffer, sizeof(response_buffer), 1000);
        if (len > 0) {
            printf("   <<< 收到: %s", response_buffer);
            if (strstr(response_buffer, expected_response) != NULL) {
                return 1;
            }
        }
        HAL_Delay(100);
    }
    return 0;
}

// 发送AT命令并等待OK响应
static uint8_t send_at_command_and_wait_ok(const char* command, uint32_t timeout_ms) {
    if (command == NULL) {
        printf("   错误: 命令为空\r\n");
        return 0;
    }
    
    printf("   >>> 发送: %s\r\n", command);
    ESP_UART_ClearRxBuffer();
    ESP_UART_SendString(command);
    
    return check_response_contains(ESP_RESPONSE_OK, timeout_ms);
}

// 发送AT命令并等待特定响应
static uint8_t send_at_command_and_wait_response(const char* command, const char* expected_response, uint32_t timeout_ms) {
    if (command == NULL || expected_response == NULL) {
        printf("   错误: 命令或期望响应为空\r\n");
        return 0;
    }
    
    printf("   >>> 发送: %s\r\n", command);
    printf("   >>> 等待响应: %s\r\n", expected_response);
    ESP_UART_ClearRxBuffer();
    ESP_UART_SendString(command);
    
    return check_response_contains(expected_response, timeout_ms);
}

// ===== 模块化函数 =====

// 基础初始化
ESP_Result_t ESP_BasicInit(void) {
    printf("\r\n=== ESP基础初始化 ===\r\n");

    // 关闭命令回显
    printf("1. 关闭命令回显...\r\n");
    if (send_at_command_and_wait_ok(ESP_CMD_ATE0, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ 回显已关闭\r\n");
    } else {
        printf("   ✗ 关闭回显失败\r\n");
        return ESP_ERROR;
    }

    // AT测试
    printf("2. AT连接测试...\r\n");
    if (send_at_command_and_wait_ok(ESP_CMD_AT, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ AT测试成功\r\n");
    } else {
        printf("   ✗ AT测试失败\r\n");
        return ESP_ERROR;
    }

    printf("=== ESP基础初始化完成 ===\r\n\r\n");
    return ESP_SUCCESS;
}

// WiFi设置和连接
ESP_Result_t ESP_WiFiSetupAndConnect(void) {
    char command_buffer[128];
    
    printf("\r\n=== WiFi配置和连接 ===\r\n");

    // 设置WiFi模式为客户端模式
    printf("1. 设置WiFi客户端模式...\r\n");
    if (send_at_command_and_wait_ok(ESP_CMD_CWMODE_SET, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ WiFi客户端模式设置成功\r\n");
    } else {
        printf("   ✗ WiFi客户端模式设置失败\r\n");
        return ESP_ERROR;
    }

    // 连接到WiFi网络
    printf("2. 连接到WiFi网络: %s\r\n", ESP_WIFI_SSID);
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_CWJAP_FORMAT, ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
    if (send_at_command_and_wait_ok(command_buffer, ESP_WIFI_CONNECT_TIMEOUT)) {
        printf("   ✓ WiFi连接成功\r\n");
    } else {
        printf("   ✗ WiFi连接失败\r\n");
        return ESP_ERROR;
    }

    printf("=== WiFi配置完成 ===\r\n\r\n");
    return ESP_SUCCESS;
}

// ===== MQTT AT指令实现 =====

// 配置MQTT用户参数
ESP_Result_t ESP_MQTT_AT_Configure(void) {
    char command_buffer[256];
    
    printf("\r\n=== 配置MQTT用户参数 ===\r\n");
    
    // 构建MQTT用户配置命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_USERCFG,
             MQTT_LINK_ID, MQTT_SCHEME, MQTT_CLIENT_ID, MQTT_USERNAME, 
             MQTT_PASSWORD, MQTT_CERT_KEY_ID, MQTT_CA_ID, MQTT_PATH);
    
    printf("配置MQTT参数:\r\n");
    printf("  - 客户端ID: %s\r\n", MQTT_CLIENT_ID);
    printf("  - 用户名: %s\r\n", MQTT_USERNAME);
    printf("  - 密码: %s\r\n", MQTT_PASSWORD);
    
    if (send_at_command_and_wait_ok(command_buffer, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ MQTT用户配置成功\r\n");
        printf("=== MQTT配置完成 ===\r\n\r\n");
        return ESP_SUCCESS;
    } else {
        printf("   ✗ MQTT用户配置失败\r\n");
        return ESP_ERROR;
    }
}

// 连接MQTT服务器
ESP_Result_t ESP_MQTT_AT_Connect(void) {
    char command_buffer[128];
    
    printf("\r\n=== 连接MQTT服务器 ===\r\n");
    
    // 构建MQTT连接命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_CONN,
             MQTT_LINK_ID, MQTT_BROKER_HOST, MQTT_BROKER_PORT, 1);
    
    printf("连接MQTT服务器: %s:%d\r\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    
    if (send_at_command_and_wait_response(command_buffer, ESP_RESPONSE_MQTTCONNECTED, 10000)) {
        printf("   ✓ MQTT服务器连接成功\r\n");
        printf("=== MQTT连接完成 ===\r\n\r\n");
        return ESP_SUCCESS;
    } else {
        printf("   ✗ MQTT服务器连接失败\r\n");
        return ESP_ERROR;
    }
}

// 订阅主题
ESP_Result_t ESP_MQTT_AT_Subscribe(const char* topic) {
    char command_buffer[128];
    
    if (topic == NULL) {
        printf("   ✗ 订阅主题不能为空\r\n");
        return ESP_ERROR;
    }
    
    printf("\r\n=== 订阅MQTT主题 ===\r\n");
    
    // 构建订阅命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_SUB,
             MQTT_LINK_ID, topic, MQTT_QOS_LEVEL);
    
    printf("订阅主题: %s (QoS: %d)\r\n", topic, MQTT_QOS_LEVEL);
    
    if (send_at_command_and_wait_ok(command_buffer, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ 主题订阅成功\r\n");
        return ESP_SUCCESS;
    } else {
        printf("   ✗ 主题订阅失败\r\n");
        return ESP_ERROR;
    }
}

// 发布消息
ESP_Result_t ESP_MQTT_AT_Publish(const char* topic, const char* message) {
    char command_buffer[256];
    
    if (topic == NULL || message == NULL) {
        printf("   ✗ 发布主题和消息不能为空\r\n");
        return ESP_ERROR;
    }
    
    printf("\r\n=== 发布MQTT消息 ===\r\n");
    
    // 构建发布命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_PUB,
             MQTT_LINK_ID, topic, message, MQTT_QOS_LEVEL, MQTT_RETAIN_FLAG);
    
    printf("发布到主题: %s\r\n", topic);
    printf("消息内容: %s\r\n", message);
    printf(">>> 发送: %s\r\n", command_buffer);
    
    // 清空接收缓冲区
    ESP_UART_ClearRxBuffer();
    
    // 发送AT指令
    ESP_UART_SendString(command_buffer);
    
    // 等待响应
    char response_buffer[128];
    uint32_t start_time = HAL_GetTick();
    
    while ((HAL_GetTick() - start_time) < ESP_RESPONSE_TIMEOUT) {
        uint16_t len = ESP_UART_ReadLine(response_buffer, sizeof(response_buffer), 1000);
        if (len > 0) {
            printf("<<< 收到: %s", response_buffer);
            
            if (strstr(response_buffer, ESP_RESPONSE_OK) != NULL) {
                printf("   ✓ 消息发布成功\r\n");
                return ESP_SUCCESS;
            } else if (strstr(response_buffer, ESP_RESPONSE_ERROR) != NULL) {
                printf("   ✗ 消息发布失败: %s", response_buffer);
                return ESP_ERROR;
            }
        }
        HAL_Delay(100);
    }
    
    printf("   ✗ 消息发布超时\r\n");
    return ESP_ERROR;
}

// 查询订阅的主题
ESP_Result_t ESP_MQTT_AT_QuerySubscriptions(void) {
    printf("\r\n=== 查询订阅主题 ===\r\n");
    
    ESP_UART_ClearRxBuffer();
    ESP_UART_SendString(ESP_CMD_MQTT_SUB_QUERY);
    
    // 读取所有响应
    char response_buffer[256];
    uint32_t start_time = HAL_GetTick();
    
    while ((HAL_GetTick() - start_time) < ESP_RESPONSE_TIMEOUT) {
        uint16_t len = ESP_UART_ReadLine(response_buffer, sizeof(response_buffer), 1000);
        if (len > 0) {
            printf("   <<< %s", response_buffer);
            if (strstr(response_buffer, ESP_RESPONSE_OK) != NULL) {
                printf("   ✓ 查询完成\r\n");
                return ESP_SUCCESS;
            }
        }
        HAL_Delay(100);
    }
    
    printf("   ✗ 查询超时\r\n");
    return ESP_ERROR;
}

// 取消订阅
ESP_Result_t ESP_MQTT_AT_Unsubscribe(const char* topic) {
    char command_buffer[128];
    
    if (topic == NULL) {
        printf("   ✗ 取消订阅主题不能为空\r\n");
        return ESP_ERROR;
    }
    
    printf("\r\n=== 取消订阅主题 ===\r\n");
    
    // 构建取消订阅命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_UNSUB,
             MQTT_LINK_ID, topic);
    
    printf("取消订阅主题: %s\r\n", topic);
    
    if (send_at_command_and_wait_ok(command_buffer, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ 取消订阅成功\r\n");
        return ESP_SUCCESS;
    } else {
        printf("   ✗ 取消订阅失败\r\n");
        return ESP_ERROR;
    }
}

// 断开MQTT连接
ESP_Result_t ESP_MQTT_AT_Disconnect(void) {
    char command_buffer[64];
    
    printf("\r\n=== 断开MQTT连接 ===\r\n");
    
    // 构建断开连接命令
    snprintf(command_buffer, sizeof(command_buffer), ESP_CMD_MQTT_CLEAN, MQTT_LINK_ID);
    
    if (send_at_command_and_wait_ok(command_buffer, ESP_RESPONSE_TIMEOUT)) {
        printf("   ✓ MQTT连接已断开\r\n");
        return ESP_SUCCESS;
    } else {
        printf("   ✗ 断开MQTT连接失败\r\n");
        return ESP_ERROR;
    }
}

// ===== 便捷的设备数据发布函数 =====

// 发布设备状态
ESP_Result_t ESP_MQTT_AT_PublishDeviceStatus(void) {
    return ESP_MQTT_AT_Publish(MQTT_TOPIC_TEST, MQTT_MESSAGE_STATUS);
}

// 发布计数器值
ESP_Result_t ESP_MQTT_AT_PublishCounter(uint32_t count) {
    char message_buffer[64];
    snprintf(message_buffer, sizeof(message_buffer), MQTT_MESSAGE_COUNTER, count);
    return ESP_MQTT_AT_Publish(MQTT_TOPIC_TEST, message_buffer);
}

// 发布温度值
ESP_Result_t ESP_MQTT_AT_PublishTemperature(float temp) {
    char message_buffer[64];
    snprintf(message_buffer, sizeof(message_buffer), MQTT_MESSAGE_TEMP, temp);
    return ESP_MQTT_AT_Publish(MQTT_TOPIC_TEST, message_buffer);
}

// ===== 完整初始化函数 =====

ESP_Result_t ESP_MQTT_FullInit(void) {
    printf("\r\n========== ESP MQTT 完整初始化开始 ==========\r\n");
    
    // 1. 基础初始化
    if (ESP_BasicInit() != ESP_SUCCESS) {
        printf("❌ 基础初始化失败\r\n");
        return ESP_ERROR;
    }
    
    // 2. WiFi连接
    if (ESP_WiFiSetupAndConnect() != ESP_SUCCESS) {
        printf("❌ WiFi连接失败\r\n");
        return ESP_ERROR;
    }
    
    // 3. MQTT配置
    if (ESP_MQTT_AT_Configure() != ESP_SUCCESS) {
        printf("❌ MQTT配置失败\r\n");
        return ESP_ERROR;
    }
    
    // 4. MQTT连接
    if (ESP_MQTT_AT_Connect() != ESP_SUCCESS) {
        printf("❌ MQTT连接失败\r\n");
        return ESP_ERROR;
    }
    
    // 5. 订阅测试主题
    if (ESP_MQTT_AT_Subscribe(MQTT_TOPIC_TEST) != ESP_SUCCESS) {
        printf("❌ 主题订阅失败\r\n");
        return ESP_ERROR;
    }
    
    // 6. 发布测试消息
    if (ESP_MQTT_AT_Publish(MQTT_TOPIC_TEST, MQTT_MESSAGE_HELLO) != ESP_SUCCESS) {
        printf("❌ 消息发布失败\r\n");
        return ESP_ERROR;
    }
    
    printf("========== ESP MQTT 完整初始化成功 ==========\r\n\r\n");
    return ESP_SUCCESS;
}















