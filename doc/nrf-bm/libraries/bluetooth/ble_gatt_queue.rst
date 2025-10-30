.. _lib_ble_gatt_queue:

Bluetooth: GATT Queue
#####################

.. contents::
   :local:
   :depth: 2

The Bluetooth Low Energy® GATT Queue library can be used to buffer BLE GATT requests if the SoftDevice is not able to handle them at the moment.

Overview
********

If the SoftDevice is not able to handle the GATT request at the moment, the BLE GATT Queue library buffers the request.
Later on, when the corresponding BLE event indicates that the SoftDevice may be free, the request is retried.
This library can be used in multiple connections scenario.

The library currently handles the following types of GATT requests:

* Read Characteristic or Descriptor (See ``sd_ble_gattc_read``)
* Write Characteristic Value or Descriptor (See ``sd_ble_gattc_write``)
* Primary Service Discovery (See ``sd_ble_gattc_primary_services_discover``)
* Characteristic Discovery (See ``sd_ble_gattc_characteristics_discover``)
* Descriptor Discovery (See ``sd_ble_gattc_descriptors_discover``)
* Notify or Indicate an attribute value. (See ``sd_ble_gatts_hvx``)

The BLE GATT Queue can be used with BLE service clients and other BLE libraries that issue GATT requests to the SoftDevice.

Configuration
*************

The library is enabled and configured using the Kconfig system.
Set the :kconfig:option:`CONFIG_BLE_GATT_QUEUE` Kconfig option to enable the library.

You can add an instance of the library to your application using the :c:macro:`BLE_GQ_DEF` macro.
If you need an instance that does not use the default parameters, it can be added with the :c:macro:`BLE_GQ_CUSTOM_DEF` macro.

The library exposes the following Kconfig options for the default configuration:

* :kconfig:option:`CONFIG_BLE_GQ_MAX_CONNECTIONS` - Sets the maximum simultaneous connections the GATT queue instance can manage.
* :kconfig:option:`CONFIG_BLE_GQ_QUEUE_SIZE` - Sets the max number of requests that can be queued for each connection that have been registered to the GATT queue instance.
* :kconfig:option:`CONFIG_BLE_GQ_HEAP_SIZE` - Sets the heap size for storing additional data that can be of variable size.
  The heap is used for storing the data of write, notify and indicate requests.

Initialization
==============

The module does not require other initialization than adding the library instance as described above.

Usage
*****

Once defined, you can register a connection handle in the BLE Gatt Queue (BLE GQ) instance by calling the :c:func:`ble_gq_conn_handle_register` function.
From this point forward, the BLE GQ instance can handle GATT requests associated with the handle until the connection is no longer valid (for example when a disconnect event occurs).

To add a GATT request to the BLE GQ instance, call the :c:func:`ble_gq_item_add` function.
This function adds a request to the BLE GQ instance and allocates the necessary memory for data that can be held within the request descriptor.
If the SoftDevice is free and the queue is empty, the request will be processed immediately.
Otherwise, the request is queued and processed later.

Dependencies
************

This library uses the following |BMshort| libraries:

* SoftDevice - :kconfig:option:`CONFIG_SOFTDEVICE`
* SoftDevice handler - :kconfig:option:`CONFIG_NRF_SDH`

API documentation
*****************

| Header file: :file:`include/bm/bluetooth/ble_gq.h`
| Source files: :file:`lib/bluetooth/ble_gq/`

:ref:`Bluetooth LE GATT Queue library API reference <api_ble_gatt_queue>`
