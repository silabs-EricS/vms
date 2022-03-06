/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_status.h"
#include "app.h"

#include "sl_btmesh_api.h"
#include "sl_bt_api.h"

#include "my_model_def.h"
#ifdef PROV_LOCALLY
#include "self_test.h"
/* The default settings of the network and the node */
#define NET_KEY_IDX                 0
#define APP_KEY_IDX                 0
#define IVI                         0
#define DEFAULT_TTL                 5
/* #define ELEMENT_ID                  0 */
#endif /* #ifdef PROV_LOCALLY */


#define RES_100_MILLI_TICKS         3277
#define RES_1_SEC_TICKS             (32768)
#define RES_10_SEC_TICKS            ((32768) * (10))
#define RES_10_MIN_TICKS            ((32768) * (60) * (10))

#define RES_100_MILLI               0
#define RES_1_SEC                   ((1) << 6)
#define RES_10_SEC                  ((2) << 6)
#define RES_10_MIN                  ((3) << 6)

#define TIMER_ID_PERIODICAL_UPDATE  50
#define RESET_TIMER_ID              62

/// Advertising Provisioning Bearer
#define PB_ADV                         0x1
/// GATT Provisioning Bearer
#define PB_GATT                        0x2

#ifdef PROV_LOCALLY
static uint16_t uni_addr = 0;

static aes_key_128 enc_key = {
  .data = "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
};
#endif /* #ifdef PROV_LOCALLY */

uint8_t temperature[TEMP_DATA_LENGTH];
unit_t unit[UNIT_DATA_LENGTH] = {
  celsius
};
uint8_t update_interval[UPDATE_INTERVAL_LENGTH] = {
  0
};

my_model_t my_model = {
  .elem_index = PRIMARY_ELEMENT,
  .vendor_id = MY_VENDOR_ID,
  .model_id = MY_MODEL_SERVER_ID,
  .publish = 1,
  .opcodes_len = NUMBER_OF_OPCODES,
  .opcodes_data[0] = temperature_get,
  .opcodes_data[1] = temperature_status,
  .opcodes_data[2] = unit_get,
  .opcodes_data[3] = unit_set,
  .opcodes_data[4] = unit_set_unack,
  .opcodes_data[5] = unit_status,
  .opcodes_data[6] = update_interval_get,
  .opcodes_data[7] = update_interval_set,
  .opcodes_data[8] = update_interval_set_unack,
  .opcodes_data[9] = update_interval_status
};

#include "sl_simple_led_instances.h"
#include "sl_simple_led.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_button.h"

extern const sl_led_t sl_led_led0;
extern sl_simple_button_context_t simple_btn0_context;

uint16_t   a_destination_address;
int8_t     a_va_index;
uint16_t   a_appkey_index;
uint8_t    a_nonrelayed;
uint8_t test = 0;
void sl_button_on_change(const sl_button_t *handle)
{
  sl_simple_button_context_t *ctxt = ((sl_simple_button_context_t *)handle[0].context);
  if (ctxt->state) {
      ctxt->history += 1;
      sl_bt_external_signal(1);
  }
}


void read_temperature(void)
{
  float temp;
  if (unit[0] == fahrenheit) {
    temp = (float) (*(int32_t *) temperature / 1000);
    temp = temp * 1.8 + 32;
    *(int32_t *) temperature = (int32_t) (temp * 1000);
  }
}

/* Delay a centain time then reset */
static void delay_reset(uint32_t ms)
{
  if (ms < 10) {
    ms = 10;
  }
  sl_bt_system_set_soft_timer(328 * (ms / 10), RESET_TIMER_ID, 1);
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(struct sl_bt_msg *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      // Initialize Mesh stack in Node operation mode,
      // wait for initialized event
      app_log("Node init\r\n");
      if(GPIO_PinInGet(gpioPortD,2) == 0){
        sc = sl_btmesh_node_reset();
        app_assert_status_f(sc, "Failed to reset node\r\n");
      }

      sc = sl_btmesh_node_init();
      app_assert_status_f(sc, "Failed to init node\n");
      break;
    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_system_soft_timer_id:
      switch (evt->data.evt_system_soft_timer.handle) {
        case RESET_TIMER_ID:
          app_log("Reset Device\r\n");
          sl_bt_system_reset(0);
          break;
        case TIMER_ID_PERIODICAL_UPDATE:
          read_temperature();
#if 1
          if(0)//(test++%2)
          {
            sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                                        my_model.vendor_id,
                                                        my_model.model_id,
                                                        temperature_status,
                                                        1,
                                                        TEMP_DATA_LENGTH,
                                                        temperature);
            if (sc) {
              app_log("Set publication error = 0x%04X\r\n", sc);
            } else {
              app_log("Set publication done. Publishing...\r\n");
              sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                                  my_model.vendor_id,
                                                  my_model.model_id);
              if (sc) {
                app_log("Publish error = 0x%04X\r\n", sc);
              } else {
                app_log("Publish done.\r\n");
              }
            }
          } else {
            sc = sl_btmesh_vendor_model_send( SERVER_PUB_ADDR/*a_destination_address*/,
                                              a_va_index,
                                              a_appkey_index,
                                              my_model.elem_index,
                                              my_model.vendor_id,
                                              my_model.model_id,
                                              a_nonrelayed,
                                              temperature_status,
                                              1,
                                              TEMP_DATA_LENGTH,
                                              temperature);
            if (sc) {
              app_log("gecko_cmd_mesh_vendor_model_send error = 0x%04X\r\n", sc);
            } else {
              app_log("gecko_cmd_mesh_vendor_model_send done.\r\n");
            }
          }
#endif
          break;
        default:
          break;
      }
      break;

    case sl_bt_evt_system_external_signal_id:
      app_log("sl_bt_evt_system_external_signal_id\r\n");
      {
        uint8_t opcode = 0, length = 0, *data = NULL;
        if (evt->data.evt_system_external_signal.extsignals == 1) {
          read_temperature();
          opcode = temperature_status;
          length = TEMP_DATA_LENGTH;
          data = temperature;
          app_log("PB0 Pressed.\r\n");
        }

        sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                                    my_model.vendor_id,
                                                    my_model.model_id,
                                                    opcode,
                                                    1,
                                                    length,
                                                    data);
        if (sc) {
          app_log("Set publication error = 0x%04X\r\n", sc);
        } else {
          app_log("Set publication done. Publishing...\r\n");
          sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                              my_model.vendor_id,
                                              my_model.model_id);
          if (sc) {
            app_log("Publish error = 0x%04X\r\n", sc);
          } else {
            app_log("Publish done.\r\n");
          }
        }
      }
      break;
    // -------------------------------
    // Default event handler.
    default:
      //app_log("BLE unhandled evt: %8.8x class %2.2x method %2.2x\r\n", evt->header, (evt->header >> 16) & 0xFF, (evt->header >> 24) & 0xFF);
      break;
  }
}

/**************************************************************************//**
 * Bluetooth Mesh stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth Mesh stack.
 *****************************************************************************/
void sl_btmesh_on_event(sl_btmesh_msg_t *evt)
{
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_btmesh_evt_node_initialized_id:
    {
      // The Node is now initialized,
      // start unprovisioned Beaconing using PB-ADV and PB-GATT Bearers
      app_log("Initialized\r\n");


      sc = sl_btmesh_vendor_model_init(my_model.elem_index,
                                       my_model.vendor_id,
                                       MY_MODEL_SERVER_ID,
                                       my_model.publish,
                                       my_model.opcodes_len,
                                       my_model.opcodes_data);
      if (sc) {
        app_log("Vendor model init error = 0x%04X\r\n", sc);
      } else {
        app_log("Vendor model init done. --- ");
        app_log("Server. \r\n");
      }
      if (evt->data.evt_node_initialized.provisioned) {
        app_log("Node provisioned already.\r\n");
        } else {
        app_log("Node unprovisioned.\r\n");
        #ifdef PROV_LOCALLY
        /* Derive the unicast address from the LSB 2 bytes from the BD_ADDR */
        bd_addr *   addr;
        uint8_t address[8];
        sc = sl_bt_system_get_identity_address(addr, 0);
        if (sc) {
          app_log("Get address error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to get address\n");
        }
        memcpy(address, addr,sizeof(bd_addr));
        uni_addr = ((address[1] << 8) | address[0]) & 0x7FFF;
        app_log("Unicast Address = 0x%04X\r\n", uni_addr);
        app_log("Provisioning itself.\r\n");
        sc = sl_btmesh_node_set_provisioning_data(enc_key,
                                                  enc_key,
                                                  NET_KEY_IDX,
                                                  IVI,
                                                  uni_addr,
                                                  0);
        if (sc) {
          app_log("Set provisioning data error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set provisioning data\n");
        }else{
          app_log("Set provisioning data OK, Reset\r\n");
        }
        delay_reset(100);
        break;
        #else
        app_log("Sending unprovisioned device beacon\r\n");
        sc = sl_btmesh_node_start_unprov_beaconing(PB_ADV | PB_GATT);
        app_assert_status_f(sc, "Failed to start unprovisioned beaconing\n");
        #endif /* #ifdef PROV_LOCALLY */
      }
      #ifdef PROV_LOCALLY
      /* Set the publication and subscription */
      uint16_t *  appkey_index = 0;
      uint16_t *  pub_address = 0;
      uint8_t *   ttl = 0;
      uint8_t *   period = 0;
      uint8_t *   retrans = 0;
      uint8_t *   credentials = 0;
      sc= sl_btmesh_test_get_local_model_pub(my_model.elem_index,
                                             my_model.vendor_id,
                                             my_model.model_id,
                                             appkey_index,
                                             pub_address,
                                             ttl,
                                             period,
                                             retrans,
                                             credentials);
      if ((!sc) && (pub_address == SERVER_PUB_ADDR)) {
        app_log("Configuration done already.\r\n");
      } else {
        app_log("Pub setting result = 0x%04X, pub setting address = 0x%04X\r\n", sc, pub_address);
        app_log("Add local app key...\r\n");
        sc = sl_btmesh_test_add_local_key(1,
                                                  enc_key,
                                                  APP_KEY_IDX,
                                                  NET_KEY_IDX);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }

        app_log("Bind local app key...\r\n");
        sc = sl_btmesh_test_bind_local_model_app(my_model.elem_index,
                                                         APP_KEY_IDX,
                                                         my_model.vendor_id,
                                                         my_model.model_id);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }

        app_log("Set local model pub...\r\n");
        sc = sl_btmesh_test_set_local_model_pub(my_model.elem_index,
                                                        APP_KEY_IDX,
                                                        my_model.vendor_id,
                                                        my_model.model_id,
                                                        SERVER_PUB_ADDR,
                                                        DEFAULT_TTL,
                                                        0, 0, 0);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }

        app_log("Add local model sub...\r\n");
        sc = sl_btmesh_test_add_local_model_sub(my_model.elem_index,
                                                        my_model.vendor_id,
                                                        my_model.model_id,
                                                        SERVER_SUB_ADDR);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }

        app_log("Set relay...\r\n");
        sc = sl_btmesh_test_set_relay(1, 0, 0);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }

        app_log("Set Network tx state.\r\n");
        sc = sl_btmesh_test_set_nettx(2, 4);
        if (sc) {
          app_log("error = 0x%04X\r\n", sc);
          app_assert_status_f(sc, "Failed to set test command\n");
        }else{
          app_log("OK\r\n");
        }
      }
      #endif /* #ifdef PROV_LOCALLY */
    }
      break;
    case sl_btmesh_evt_vendor_model_receive_id:
    {
      sl_btmesh_evt_vendor_model_receive_t *recv_evt = (sl_btmesh_evt_vendor_model_receive_t *) &evt->data;
      uint8_t action_req = 0, opcode = 0, payload_len = 0, *payload_data;
      payload_data = temperature;

      a_destination_address = recv_evt->source_address;
      a_va_index = recv_evt->va_index;
      a_appkey_index = recv_evt->appkey_index;
      a_nonrelayed = recv_evt->nonrelayed;

      app_log("Vendor model data received.\r\n\t"
              "Element index = %d\r\n\t"
              "Vendor id = 0x%04X\r\n\t"
              "Model id = 0x%04X\r\n\t"
              "Source address = 0x%04X\r\n\t"
              "Destination address = 0x%04X\r\n\t"
              "Destination label UUID index = 0x%02X\r\n\t"
              "App key index = 0x%04X\r\n\t"
              "Non-relayed = 0x%02X\r\n\t"
              "Opcode = 0x%02X\r\n\t"
              "Final = 0x%04X\r\n\t"
              "Payload: \r\n",
              recv_evt->elem_index,
              recv_evt->vendor_id,
              recv_evt->model_id,
              recv_evt->source_address,
              recv_evt->destination_address,
              recv_evt->va_index,
              recv_evt->appkey_index,
              recv_evt->nonrelayed,
              recv_evt->opcode,
              recv_evt->final);
      UINT8_ARRAY_DUMP(recv_evt->payload.data, recv_evt->payload.len);
      app_log("payload len:%d\r\n",recv_evt->payload.len);
      sl_led_toggle(&sl_led_led0);
      switch (recv_evt->opcode) {
        // Server
        case temperature_get:
          app_log("Sending/publishing temperature status as response to "
               "temperature get from client...\r\n");
          read_temperature();
          action_req = ACK_REQ;
          opcode = temperature_status;
          payload_len = TEMP_DATA_LENGTH;
          payload_data = temperature;
          break;
        // Server
        case unit_get:
          app_log("Sending/publishing unit status as response to unit get from "
               "client...\r\n");
          action_req = ACK_REQ;
          opcode = unit_status;
          payload_len = UNIT_DATA_LENGTH;
          payload_data = (uint8_t *) unit;
          break;
        // Server
        case unit_set:
          app_log("Sending/publishing unit status as response to unit set from "
               "client...\r\n");
          memcpy(unit, recv_evt->payload.data, recv_evt->payload.len);
          action_req = ACK_REQ | STATUS_UPDATE_REQ;
          opcode = unit_status;
          payload_len = UNIT_DATA_LENGTH;
          payload_data = (uint8_t *) unit;
          break;
        // Server
        case unit_set_unack:
          app_log("Publishing unit status as response to unit set unacknowledge "
               "from client...\r\n");
          memcpy(unit, recv_evt->payload.data, recv_evt->payload.len);
          action_req = STATUS_UPDATE_REQ;
          opcode = unit_status;
          payload_len = UNIT_DATA_LENGTH;
          payload_data = (uint8_t *) unit;
          break;

        case update_interval_get:
          app_log("Publishing Update Interval status as response to Update "
               "interval get from client...\r\n");
          action_req = ACK_REQ;
          opcode = update_interval_status;
          payload_len = UPDATE_INTERVAL_LENGTH;
          payload_data = update_interval;
          break;
        case update_interval_set:
          app_log("Publishing Update Interval status as response to "
               "update_interval_set from client...\r\n");
          memcpy(update_interval,
                 recv_evt->payload.data,
                 recv_evt->payload.len);
          action_req = ACK_REQ | STATUS_UPDATE_REQ;
          opcode = update_interval_status;
          payload_len = UPDATE_INTERVAL_LENGTH;
          payload_data = update_interval;
//          setup_periodcal_update(update_interval[0]);
          break;
        case update_interval_set_unack:
          app_log("Publishing Update Interval status as response to "
               "update_interval_set_unack from client...\r\n");
          memcpy(update_interval,
                 recv_evt->payload.data,
                 recv_evt->payload.len);
          action_req = STATUS_UPDATE_REQ;
          opcode = update_interval_status;
          payload_len = UPDATE_INTERVAL_LENGTH;
          payload_data = update_interval;
//          setup_periodcal_update(update_interval[0]);
          break;
        default:
          break;
      }

      if (action_req & ACK_REQ) {
        sc = sl_btmesh_vendor_model_send( recv_evt->source_address,
                                          recv_evt->va_index,
                                          recv_evt->appkey_index,
                                          my_model.elem_index,
                                          my_model.vendor_id,
                                          my_model.model_id,
                                          recv_evt->nonrelayed,
                                          opcode,
                                          1,
                                          payload_len,
                                          payload_data);
        if (sc) {
          app_log("gecko_cmd_mesh_vendor_model_send error = 0x%04X\r\n", sc);
        } else {
          app_log("gecko_cmd_mesh_vendor_model_send done.\r\n");
        }
        sl_bt_system_set_soft_timer(32768/5, TIMER_ID_PERIODICAL_UPDATE, 0);
      }
      if (action_req & STATUS_UPDATE_REQ) {
        sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                                    my_model.vendor_id,
                                                    my_model.model_id,
                                                    opcode,
                                                    1,
                                                    payload_len,
                                                    payload_data);
        if (sc) {
          app_log("Set publication error = 0x % 04X\r\n", sc);
        } else {
          app_log("Set publication done.Publishing ...\r\n");
          sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                                        my_model.vendor_id,
                                                        my_model.model_id);
          if (sc) {
            app_log("Publish error = 0x % 04X\r\n", sc);
          } else {
            app_log("Publish done.\r\n");
          }
        }
      }
    }
      break;
    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_btmesh_evt_node_provisioned_id: {
      app_log("Provisioning done.\r\n");
    }
    break;

    case sl_btmesh_evt_node_provisioning_failed_id:
      app_log("Provisioning failed. Result = 0x%04x\r\n",
           evt->data.evt_node_provisioning_failed.result);
      break;

    case sl_btmesh_evt_node_provisioning_started_id:
      app_log("Provisioning started.\r\n");
      break;

    case sl_btmesh_evt_node_key_added_id:
      app_log("got new %s key with index %x\r\n",
           evt->data.evt_node_key_added.type == 0 ? "network" : "application",
           evt->data.evt_node_key_added.index);
      break;

    case sl_btmesh_evt_node_config_set_id: {
      app_log("gecko_evt_mesh_node_config_set_id\r\n\t");
//      sl_btmesh_evt_node_config_set_t *set_evt = (sl_btmesh_evt_node_config_set_t *) &evt->data;
//      UINT8_ARRAY_DUMP(set_evt->value.data, set_evt->value.len);
    }
    break;
    case sl_btmesh_evt_node_model_config_changed_id :
      app_log("model config changed\r\n");
      break;
    // -------------------------------
    // Default event handler.
    default:
      app_log("BTmesh unhandled evt: %8.8x class %2.2x method %2.2x\r\n", evt->header, (evt->header >> 16) & 0xFF, (evt->header >> 24) & 0xFF);
      break;
  }
}
