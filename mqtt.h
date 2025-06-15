#ifndef serial_H
#define serial_H

#include "main.h"
#include "string.h"
#include "stdio.h"

// ESP配置相关宏
#define ESP_RESPONSE_TIMEOUT    5000    // ESP响应超时时间(毫秒)
#define ESP_WIFI_CONNECT_TIMEOUT 10000  // WiFi连接超时时间(毫秒)
#define ESP_WIFI_SSID          "Dxny"     // WiFi网络名称
#define ESP_WIFI_PASSWORD      "dxfly211"     // WiFi密码
#define ESP_MAX_RETRY_COUNT    3        // 最大重试次数

// 基础 ESP AT指令宏
#define ESP_CMD_ATE0           "ATE0"
#define ESP_CMD_AT             "AT"
#define ESP_CMD_RST            "AT+RST"
#define ESP_CMD_CWMODE_QUERY   "AT+CWMODE?"
#define ESP_CMD_CWMODE_SET     "AT+CWMODE=1"
#define ESP_CMD_CWDHCP_SET     "AT+CWDHCP=1,1"
#define ESP_CMD_CWJAP_FORMAT   "AT+CWJAP=\"%s\",\"%s\""

// MQTT AT指令配置宏 - 根据用户图片参数配置
#define MQTT_BROKER_HOST       "47.113.191.144"    // MQTT服务器地址
#define MQTT_BROKER_PORT       1883                // MQTT服务器端口
#define MQTT_LINK_ID           0                   // 连接ID
#define MQTT_SCHEME            1                   // 协议方案 (1=MQTT)
#define MQTT_CLIENT_ID         "ID"     // MQTT客户端ID
#define MQTT_USERNAME          "MyName"            // MQTT用户名
#define MQTT_PASSWORD          "272935657"          // MQTT密码
#define MQTT_CERT_KEY_ID       0                   // 证书密钥ID
#define MQTT_CA_ID             0                   // CA证书ID
#define MQTT_PATH              ""                  // 路径(为空)

// MQTT主题配置
#define MQTT_TOPIC_TEST        "abc"        // 测试主题
#define MQTT_QOS_LEVEL         0                   // QoS等级
#define MQTT_RETAIN_FLAG       0                   // 保留标志

// MQTT AT指令宏
#define ESP_CMD_MQTT_USERCFG   "AT+MQTTUSERCFG=%d,%d,\"%s\",\"%s\",\"%s\",%d,%d,\"%s\""
#define ESP_CMD_MQTT_CONN      "AT+MQTTCONN=%d,\"%s\",%d,%d"
#define ESP_CMD_MQTT_SUB       "AT+MQTTSUB=%d,\"%s\",%d"
#define ESP_CMD_MQTT_PUB       "AT+MQTTPUB=%d,\"%s\",\"%s\",%d,%d"
#define ESP_CMD_MQTT_UNSUB     "AT+MQTTUNSUB=%d,\"%s\""
#define ESP_CMD_MQTT_CLEAN     "AT+MQTTCLEAN=%d"
#define ESP_CMD_MQTT_SUB_QUERY "AT+MQTTSUB?"

// 设备数据模板
#define MQTT_MESSAGE_HELLO     "Hello from ESP8266"
#define MQTT_MESSAGE_STATUS    "Device Status: Online"
#define MQTT_MESSAGE_COUNTER   "Counter: %d"
#define MQTT_MESSAGE_TEMP      "Temperature: %.1fC"

// 响应字符串宏
#define ESP_RESPONSE_OK        "OK"
#define ESP_RESPONSE_ERROR     "ERROR"
#define ESP_RESPONSE_FAIL      "FAIL"
#define ESP_RESPONSE_MQTTCONNECTED "MQTTCONNECTED"
#define ESP_RESPONSE_MQTTSUBRECV   "MQTTSUBRECV"
#define ESP_RESPONSE_MQTTDISCONNECTED "MQTTDISCONNECTED"

// 返回值枚举
typedef enum {
    ESP_SUCCESS = 0,
    ESP_ERROR   = 1
} ESP_Result_t;

// 基础UART函数
HAL_StatusTypeDef ESP_UART_Init(UART_HandleTypeDef *huart_handle);
void ESP_UART_SendString(const char* cmd);
void ESP_UART_SendBytes(const uint8_t* data, uint16_t len);
int16_t ESP_UART_ReadByte(void);
uint16_t ESP_UART_ReadLine(char* line_buffer, uint16_t buffer_capacity, uint32_t timeout_ms);
void ESP_UART_ClearRxBuffer(void);

// 模块化函数
ESP_Result_t ESP_BasicInit(void);           // 基础AT测试和回显禁用
ESP_Result_t ESP_WiFiSetupAndConnect(void); // WiFi设置和连接

// AT指令方式MQTT实现 (主要使用)
ESP_Result_t ESP_MQTT_AT_Configure(void);                          // 配置MQTT用户参数
ESP_Result_t ESP_MQTT_AT_Connect(void);                            // 连接MQTT服务器
ESP_Result_t ESP_MQTT_AT_Subscribe(const char* topic);             // 订阅主题
ESP_Result_t ESP_MQTT_AT_Publish(const char* topic, const char* message); // 发布消息
ESP_Result_t ESP_MQTT_AT_QuerySubscriptions(void);                 // 查询订阅的主题
ESP_Result_t ESP_MQTT_AT_Unsubscribe(const char* topic);          // 取消订阅
ESP_Result_t ESP_MQTT_AT_Disconnect(void);                         // 断开MQTT连接

// 便捷的设备数据发布函数
ESP_Result_t ESP_MQTT_AT_PublishDeviceStatus(void);    // 发布设备状态
ESP_Result_t ESP_MQTT_AT_PublishCounter(uint32_t count); // 发布计数器值
ESP_Result_t ESP_MQTT_AT_PublishTemperature(float temp); // 发布温度值

// 完整初始化函数
ESP_Result_t ESP_MQTT_FullInit(void);

#endif








