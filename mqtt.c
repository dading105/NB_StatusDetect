/*
** File   : mqttdemo.c
**
** Copyright (C) 2013-2018 Gosuncn. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**      http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** mqtt-demo说明：此demo为 mqtt 协议的应用实例，包括创建一个 mqtt 客户端并配置 mqtt 服务器，
**                发送 mqtt 链接、订阅、发布、取消订阅等报文给 mqtt 服务器，最后，通过发送   
**                断开mqtt连接报文来断开整条mqtt链路。mqtt应用实例需要模组先驻网。
**
** Author : chengkai2@gosuncn.cn
**
** $Date: 2018/11/01 11:29:36GMT+08:00 $
**
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gsdk_api.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "gsdk_ril.h"
#include "lwip/sockets.h"
#include "lwip/ip.h"

#define FLASH_TEST_BASE    0x002BD000
#define FLASH_TEST_SIZE    0x1000

static const char gsdk_test_data[] =
    "11111111,255.255.55.255,1883";

char mqttserver[32] = "255.255.55.255";
char mqttport[16] = "1883";
char mqttid[32] = "11111111";

static gsdk_handle_t g_huart;
static int mqtt_id = -1;

uint32_t statusconfig=0;
#define SUBstatus 0x00000001
#define PARAstatus 0x00000002

int setup(void)
{
    char buf[sizeof(gsdk_test_data)];
    gsdk_status_t status;
    gsdk_handle_t hflash;
    char * str1;
    char * str2;
    char * str3;

    status = gsdk_flash_open(FLASH_TEST_BASE, FLASH_TEST_SIZE, &hflash);
    if (status != GSDK_SUCCESS)
        goto _fail;
//    status = gsdk_flash_erase(hflash, FLASH_TEST_BASE, FLASH_BLOCK_4K);
//    if (status != GSDK_SUCCESS)
//        goto _fail_close;
//    status = gsdk_flash_write(hflash, FLASH_TEST_BASE, (uint8_t *)gsdk_test_data, sizeof(gsdk_test_data));
//    if (status != GSDK_SUCCESS)
//        goto _fail_close;
    status = gsdk_flash_read(hflash, FLASH_TEST_BASE, (uint8_t *)buf, sizeof(gsdk_test_data));
    if (status != GSDK_SUCCESS)
        goto _fail_close;
//    if (strcmp(gsdk_test_data, buf)) {
//        goto _fail_close;
//    }
    str1 = strtok((char *)buf,",");
    str2 = strtok(NULL,",");
    str3 = strtok(NULL,",");
    if((str1!=NULL)&&(str2!=NULL)&&(str3!=NULL))
    {
        strcpy(mqttid,str1);
        strcpy(mqttserver,str2);
        strcpy(mqttport,str3);
    }
    else
    {
    status = gsdk_flash_erase(hflash, FLASH_TEST_BASE, FLASH_BLOCK_4K);
    if (status != GSDK_SUCCESS)
        goto _fail_close;
    status = gsdk_flash_write(hflash, FLASH_TEST_BASE, (uint8_t *)gsdk_test_data, sizeof(gsdk_test_data));
    if (status != GSDK_SUCCESS)
        goto _fail_close;
    status = gsdk_flash_read(hflash, FLASH_TEST_BASE, (uint8_t *)buf, sizeof(gsdk_test_data));
    if (status != GSDK_SUCCESS)
        goto _fail_close;
    if (strcmp(gsdk_test_data, buf)) {
        goto _fail_close;
    }
    }
    
    gsdk_flash_close(hflash);
    printf("[FLASH_DEMO] Flash test pass!\n");
    return GSDK_SUCCESS;

_fail_close:
    gsdk_flash_close(hflash);
_fail:
    printf("[FLASH_DEMO] Flash test failed!\n");

    return -1;
}
int __io_puts(char *data, int len)
{
    int ret = 0;
    if (g_huart) {
        ret = gsdk_uart_write(g_huart, (uint8_t *)data, len, 1000);
    }
    return ret;
}

int log_init(void)
{
    uart_config_t uart_config;
    gsdk_status_t status;

    uart_config.baudrate    = UART_BAUDRATE_2400;
    uart_config.parity      = UART_PARITY_EVEN;
    uart_config.stop_bit    = UART_STOP_BIT_1;
    uart_config.word_length = UART_WORD_LENGTH_8;

    status = gsdk_uart_open(UART_2, &uart_config, &g_huart);
    if (status != GSDK_SUCCESS) {
        //syslog_printf(": failed to open uart %d\n", status);
        return -1;
    }
    OEM_LOGI("log init....OK\r\n");

    return 0;
}

int gsdk_ril_init_func(void)
{
    gsdk_status_t status;
    status = gsdk_ril_init();
    if (status != GSDK_SUCCESS) {
        OEM_LOGE("gsdk_ril_init error!\r\n");
        return -1;
    }
    return 0;
}

int init_func()
{
    if(log_init() != 0)
        goto init_fail;
    if(gsdk_ril_init_func() != 0)
        goto init_fail;

    OEM_LOGI("***************************************************\r\n");
    OEM_LOGI("*                                                  \r\n");
    OEM_LOGI("*                        mqtt demo                 \r\n");
    OEM_LOGI("*                                                  \r\n");
    OEM_LOGI("***************************************************\r\n");

    OEM_LOGI("init_func end!\r\n");
    return 0;
init_fail:

    OEM_LOGE("init_func error!\r\n");
    return -1;
}

int mqtt_new_func(void)
{
    char server[32] = "39.100.229.63";
    char port[16] = "1883";
    unsigned int timeout = 50000;
    unsigned int bufsize = 100;

    mqtt_new mqtt_s;
    memset(&mqtt_s, 0x0, sizeof(mqtt_new));
    mqtt_s.server_t = server;
    mqtt_s.port_t = port;
    mqtt_s.timeout_t = timeout;
    mqtt_s.bufsize_t = bufsize;

    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_new_result(&mqtt_s, &mqtt_id)){
        OEM_LOGE("mqtt new error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt new end!\r\n");
    return 0;
}

int mqtt_connect_func(void)
{
    /********* mqtt con ***********/
    int mqtt_ver = 4;
    char client_id[32] = "myclientid";
    int keepalive = 1000;
    int cleansession = 1;
    int will_flag = 0;
    char will_options[64] = {0};
    char username[16] = "yxd";
    char password[16] = "12341234";

    mqtt_con mqtt_con_c;
    memset(&mqtt_con_c, 0x0, sizeof(mqtt_con));

    mqtt_con_c.mqtt_id_t = mqtt_id;
    mqtt_con_c.mqtt_ver_t = mqtt_ver;
    mqtt_con_c.client_id_t = client_id;
    mqtt_con_c.keepalive_t = keepalive;
    mqtt_con_c.cleansession_t = cleansession;
    mqtt_con_c.will_flag_t = will_flag;
    mqtt_con_c.will_options_t = will_options;
    mqtt_con_c.username_t = username;
    mqtt_con_c.password_t = password;

    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_con_result(&mqtt_con_c)){
        OEM_LOGE("mqtt connect error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt connect end!\r\n");
    return 0;
}

int mqtt_subscribe_func(void)
{
    /********* mqtt sub **********/
    char topic[32] = "/moekon/substopic";
    int Qos = 0;

    mqtt_sub_unsub_t mqtt_sub_s;
    memset(&mqtt_sub_s, 0x0, sizeof(mqtt_sub_unsub_t));

    mqtt_sub_s.mqtt_id_t = mqtt_id;
    mqtt_sub_s.topic_t = topic;
    mqtt_sub_s.Qos_t = Qos;

    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_sub_result(&mqtt_sub_s)){
        OEM_LOGE("mqtt subscribe error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt subscribe end!\r\n");
    return 0;
}

int mqtt_publish_func(void)
{
    /************* mqtt pub ************/
    char topic2[32] = "/moekon/substopic";
    int Qos2 = 1;
    int retained = 0;
    int dup = 0;
    int message_len = 4;
    char message[128] = "31323334";

    mqtt_pub mqtt_pub_p;
    memset(&mqtt_pub_p, 0x0, sizeof(mqtt_pub));

    mqtt_pub_p.mqtt_id_t = mqtt_id;
    mqtt_pub_p.topic_t = topic2;
    mqtt_pub_p.Qos_t = Qos2;
    mqtt_pub_p.retained_t = retained;
    mqtt_pub_p.dup_t = dup;
    mqtt_pub_p.message_len_t = message_len;
    mqtt_pub_p.message_t = message;

    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_pub_result(&mqtt_pub_p)){
        OEM_LOGE("mqtt publish error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt publish end!\r\n");
    return 0;
}

int mqtt_unsubscribe_func(void)
{
    /************ mqtt unsub start **********/
    char topic3[32] = "/moekon/substopic";

    mqtt_sub_unsub_t mqtt_sub_unsub;
    memset(&mqtt_sub_unsub, 0x0, sizeof(mqtt_sub_unsub_t));

    mqtt_sub_unsub.mqtt_id_t = mqtt_id;
    mqtt_sub_unsub.topic_t = topic3;

    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_unsub_result(&mqtt_sub_unsub)){
        OEM_LOGE("mqtt unsubscribe error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt unsubscribe end!\r\n");
    return 0;
}

int mqtt_disconnect_func(void)
{
    /************ mqtt discon start **********/
    if (GSDK_SUCCESS != gsdk_ril_get_mqtt_discon_result(mqtt_id)){
        OEM_LOGE("mqtt disconnect error!\r\n");
        return -1;
    }
    OEM_LOGI("mqtt disconnect end!\r\n");
    return 0;
}

void mqtt_pub_cb(const char *s, const char * extra, int len, void *user_data)
{
    char mqtt_pub_cb_buf[128] = {0};
    if(NULL == s){
        OEM_LOGE("callbace error!\r\n");
        return;
    }
    strcpy(mqtt_pub_cb_buf, s);
    OEM_LOGI("%s\r\n", mqtt_pub_cb_buf);
}

static unsolicited_handler_t mqtt_pub_handler;
void mqtt_pub_unsolicited_func(void)
{
    mqtt_pub_handler.prefix = "+EMQPUB:";
    mqtt_pub_handler.user_data = NULL;
    mqtt_pub_handler.func = (unsolicited_handler_func)mqtt_pub_cb;
    gsdk_at_register_unsolicited_handler(&mqtt_pub_handler);
}

int ip_wait_func()
{
    int count = 0;
    gsdk_status_t status;
    char ip_buf[64] = {0};

    while(1){
        if(count > 100){
            OEM_LOGE("wait for IP timeout\r\n");
            return -1;
        }
        memset(ip_buf, 0, 64);
        status = gsdk_ril_get_local_ipaddr(ip_buf, 64);
        if(status == 1){
            OEM_LOGI("get ip success!\r\n");
            break;
        }
        OEM_LOGE("get ip failed!\r\n");
        vTaskDelay(100);
        count++;
    }
    OEM_LOGI("ip registered\r\n");
    return 0;
}

int main(void)
{
    /* system init */
    if(init_func() != 0) goto _fail;
    
    /* flash init */
    if(setup() !=0) goto _fail;

    /* wait for register network */
    if(ip_wait_func() != 0) goto _fail;

    /* mqtt publish unsolicited init */
    mqtt_pub_unsolicited_func();

    /* create a mqtt client instance and config mqtt server */
    if(mqtt_new_func() != 0) goto _fail;

    /* send mqtt connect message to server */
    if(mqtt_connect_func() != 0) goto _fail;

    /* send mqtt subscribe message to server */
    if(mqtt_subscribe_func() != 0) goto _fail;

    /* send mqtt publish message to server */
    if(mqtt_publish_func() != 0) goto _fail;

    vTaskDelay(3 * 1000 / portTICK_RATE_MS);    // delay 3s

    /* send mqtt unsubscribe message to server */
    if(mqtt_unsubscribe_func() != 0) goto _fail;

    /* send mqtt disconnect message to server */
    if(mqtt_disconnect_func() != 0) goto _fail;
    
    OEM_LOGI("mqtt demo over!!!\r\n");
    while(1){
        vTaskDelay(100);
    }

_fail:

    OEM_LOGE("main function abnormal exit!\r\n");
    return 0;
}

