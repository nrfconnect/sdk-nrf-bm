.. _lib_ble_conn_params:

Bluetooth: Connection Parameters
################################

.. contents::
   :local:
   :depth: 2

The Bluetooth Low Energy® connection parameter library handles parameter negotiation procedures while the device is in a connection and can be used to initiate connection parameter update procedures.

Overview
********

The Bluetooth LE connection parameter library provides functionality for responding to and requesting for:

* Connection parameter update
* ATT MTU exchange
* Data length update (data length extension)
* Radio PHY mode update

The library registers as a Bluetooth LE event observer using the :c:macro:`NRF_SDH_BLE_OBSERVER` macro and handles the relevant Bluetooth LE events from the SoftDevice.

Configuration
*************

The library is enabled and configured entirely using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_CONN_PARAMS` Kconfig option to enable the library.

The automatic handling for each of the four procedures can be individually disabled and enabled using the following configuration options:

* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_AUTO_GAP_CONN_PARAM_UPDATE` - Enables the connection parameter update functionality.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_AUTO_ATT_MTU` - Enables the ATT MTU exchange functionality.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_AUTO_DATA_LENGTH` - Enables the data length update functionality.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_AUTO_PHY_UPDATE` - Enables the radio PHY mode update functionality.

All of these functionalities are enabled by default and depend on the :kconfig:option:`CONFIG_BLE_CONN_PARAMS` Kconfig option being enabled.

  .. note::
     If you wish to disable all or part of the automatic handling, you must handle the respective SoftDevice events and invoke the respective SoftDevice supervisor calls yourself.

Initialization
==============

No initialization is needed, however a callback can be registered with the :c:func:`ble_conn_params_evt_handler_set` function to receive events when the parameters are updated.

Usage
*****

The following section describes the usage of this library in the four scenarios mentioned in `Configuration`_.

Connection parameter update
===========================

You can specify the connection parameters using Kconfig options:

* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_MIN_CONN_INTERVAL` - Minimum connection interval in 1.25 ms units.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_MAX_CONN_INTERVAL` - Maximum connection interval in 1.25 ms units.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PERIPHERAL_LATENCY` - Peripheral latency (number of connection events that the peripheral can skip).
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_MAX_PERIPHERAL_LATENCY_DEVIATION` - Allowed deviation from the specified peripheral latency.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_SUP_TIMEOUT` - Supervision timeout in 10 ms units.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_MAX_SUP_TIMEOUT_DEVIATION` - Allowed deviation from the specified supervision timeout.

If the device has the peripheral role in a connection and the connection parameters are not within the specified range, then the library will request a renegotiation.
The number of renegotiation attempts is configured by the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_NEGOTIATION_RETRIES` Kconfig option.

If the peripheral and central device cannot agree on a set of connection parameters after the given number of negotiation attempts, and the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_DISCONNECT_ON_FAILURE` Kconfig option is set, the device disconnects automatically.

A connection parameter update procedure can be started using the :c:func:`ble_conn_params_override` function.

When a connection parameter update procedure finishes, the Bluetooth LE connection parameter library event :c:enum:`BLE_CONN_PARAMS_EVT_UPDATED` is raised.
If the update procedure fails, the :c:enum:`BLE_CONN_PARAMS_EVT_REJECTED` event is raised instead.

ATT MTU exchange
================

If a higher ATT MTU size than the default ``23`` is required, the :kconfig:option:`CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE` Kconfig option must be set to a higher value.
This value is used in the SoftDevice configuration as part of the :c:func:`nrf_sdh_ble_enable` function and is the specified ATT MTU used by this library when doing an ATT MTU exchange.

.. note::

   * The largest ATT MTU that can fit within the largest size PDU (data length of 251) is ``247``.
   * The largest ATT MTU that can fit within two PDUs with the largest data length is ``498``.
   * The largest practical ATT MTU when writing the value of a Bluetooth LE attribute is ``515``.
     The maximum size of an attribute value is 512 bytes, plus 3 bytes of ATT header.
   * An ATT MTU exchange can only be initiated once.

The :kconfig:option:`CONFIG_BLE_CONN_PARAMS_INITIATE_ATT_MTU_EXCHANGE` Kconfig option can be set to automatically initiate an ATT MTU exchange on connection.

An ATT MTU exchange can be started using the :c:func:`ble_conn_params_att_mtu_set` function.
The current ATT MTU size can be found using the :c:func:`ble_conn_params_att_mtu_get` function.

When an ATT MTU exchange finishes, the Bluetooth LE connection parameter library event :c:enum:`BLE_CONN_PARAMS_EVT_ATT_MTU_UPDATED` is raised.

Data length update
==================

If a PDU data length larger than the default ``27`` is required, the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX` and :kconfig:option:`CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX` Kconfig options must be set to a higher value in the range of ``27`` to ``251``.

The :kconfig:option:`CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_TX` and :kconfig:option:`CONFIG_BLE_CONN_PARAMS_DATA_LENGTH_RX` Kconfig options set application-defined upper limits on the negotiated data length.

Note that the SoftDevice ATT MTU configuration (set by the :kconfig:option:`CONFIG_NRF_SDH_BLE_GATT_MAX_MTU_SIZE` Kconfig option) places limitations on the maximum negotiable data length.
This is due to memory efficiency in the SoftDevice.
For instance, to use the definitive max data length of ``251``, the ATT MTU needs to be configured to at least ``247``.

The :kconfig:option:`CONFIG_BLE_CONN_PARAMS_INITIATE_DATA_LENGTH_UPDATE` Kconfig option can be set to automatically initiate a data length update on connection.

A data length update procedure can be started using the :c:func:`ble_conn_params_data_length_set` function.
The current data length (PDU payload) size can be retrieved using the :c:func:`ble_conn_params_data_length_get` function.

When a data length update procedure completes, the Bluetooth LE connection parameter library event :c:enum:`BLE_CONN_PARAMS_EVT_DATA_LENGTH_UPDATED` is raised.

Radio PHY mode
==============

The radio PHY mode defaults to 1 MB per second at the start of a connection.
This can be changed by initiating a GAP radio PHY mode update procedure.
If a specific radio PHY mode is required in connections, one of the following choice options must be enabled:

* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PHY_AUTO` - The SoftDevice will automatically select the PHY mode.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PHY_1MBPS` - Default speed of 1 MB per second.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PHY_2MBPS` - Higher throughput of 2 MB per second.
* :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PHY_CODED` - Bluetooth LE Coded PHY mode (increased range and reliability of the transmission at the cost of reduced data throughput).

.. note::
   The S115 SoftDevice does not support the :kconfig:option:`CONFIG_BLE_CONN_PARAMS_PHY_CODED` Kconfig option.

The :kconfig:option:`CONFIG_BLE_CONN_PARAMS_INITIATE_PHY_UPDATE` Kconfig option can be set to automatically initiate a radio PHY update on connection.

A radio PHY mode update procedure can be started using the :c:func:`ble_conn_params_phy_radio_mode_set` function.
The current radio PHY mode can be retrieved using the :c:func:`ble_conn_params_phy_radio_mode_get` function.

When a radio PHY mode update procedure completes, the Bluetooth LE connection parameter library event :c:enum:`BLE_CONN_PARAMS_EVT_RADIO_PHY_MODE_UPDATED` is raised.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/ble_conn_params.h`
| Source files: :file:`lib/ble_conn_params/`

:ref:`Bluetooth LE Connection Parameter library API reference <api_ble_conn_params>`
