.. _bm_software_maturity:

Software maturity levels
########################

.. contents::
   :local:
   :depth: 2

|BMlong| supports its various features and components at different levels of software maturity.

Software maturity categories
****************************

The following categories are used to classify the software maturity of each feature and component:

Supported
   The feature or component is implemented and maintained, and is suitable for product development.

Not supported (--)
   The feature or component is neither implemented nor maintained, and it does not work.

Experimental
   The feature can be used for development, but it is not recommended for production.
   This means that the feature is incomplete in functionality or verification and can be expected to change in future releases.
   The feature is made available in its current state, but the design and interfaces can change between release tags.
   The feature is also labeled as experimental in Kconfig files and a build warning is generated to indicate this status.

See the following table for more details:

.. _software_maturity_table:

.. list-table:: Software maturity
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - Supported
     - Experimental
     - Not supported
   * - **Technical support**
     - Customer issues raised for features supported for developing end products on tagged |NCS| releases are handled by technical support on DevZone.
     - Customer issues raised for experimental features on tagged |NCS| releases are handled by technical support on DevZone.
     - Not available.
   * - **Bug fixing**
     - Reported critical bugs and security fixes will be resolved in both ``main`` and the latest tagged versions.
     - Bug fixes, security fixes, and improvements are not guaranteed to be applied.
     - Not available.
   * - **Implementation completeness**
     - Complete implementation of the agreed features set.
     - Significant changes may be applied in upcoming versions.
     - A feature or component is available as an implementation, but is not compatible with (or tested on) a specific device or series of products.
   * - **API definition**
     - The API definition is stable.
     - The API definition is not stable and may change.
     - Not available.
   * - **Maturity**
     - Suitable for integration in end products.

       A feature or component that is either fully complete on first commit, or has previously been labelled *experimental* and is now ready for use in end products.

     - Suitable for prototyping or evaluation.
       Not recommended to be deployed in end products.

       A feature or component that is either not fully verified according to the existing test plans or currently being developed, meaning that it is incomplete or that it may change in the future.
     - Not applicable.

   * - **Verification**
     - Fully verified according to the existing test plans.
     - Incomplete verification
     - Not applicable.

For the certification status of different features in a specific SoC, see its Compatibility Matrix in the `Nordic Semiconductor TechDocs`_.

nRF Devices in scope of |BMshort|
*********************************

A device is considered supported if at least one sample or component has been demonstrated on it in the SDK, with the software maturity level set to supported.

.. list-table:: Device support
   :header-rows: 0
   :align: center
   :widths: auto

   * - nRF54L15
     - Supported from v1.0.0
   * - nRF54L10
     - Supported from v1.0.0
   * - nRF54L05
     - Supported from v1.0.0
   * - nRF54LV10
     - Experimental
   * - nRF54LM20A
     - Experimental
   * - nRF54LS05B
     - Experimental

.. note::
   Any device not listed is considered out of scope.

Protocol support
****************

The following table indicates the software maturity levels of the support for each protocol.

.. list-table:: Protocol support
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
     - nRF54LVM20A
     - nRF54LS05B
   * - **Bluetooth®**
     - Supported
     - Supported
     - Supported
     - Experimental
     - Experimental
     - Experimental
   * - **NFC**
     - Experimental
     - Experimental
     - Experimental
     - n/a
     - --
     - n/a
   * - **ESB**
     - --
     - --
     - --
     - --
     - --
     - --

.. note::
   Protocols not listed are considered out of scope for |BMshort|.

Bluetooth features support
**************************

The supported Bluetooth features are determined by the selected SoftDevice.
The following table indicates SoftDevice compatibility per SoC:

.. list-table:: SoftDevice support
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
     - nRF54LM20A
     - nRF54LS05B
   * - **SoftDevice S115**
     - Supported
     - Supported
     - Supported
     - Experimental
     - Experimental
     - Experimental
   * - **SoftDevice S145**
     - Experimental
     - Experimental
     - Experimental
     - Experimental
     - Experimental
     - Experimental

.. note::
   The Experimental status of the S145 SoftDevice refers to its integration in the |BMlong|.
   The SoftDevice itself is production-ready.

The following table indicates Bluetooth features supported per SoftDevice:

.. list-table:: Bluetooth Features
   :header-rows: 1
   :align: center
   :widths: auto

   * - Feature
     - SoftDevice S115
     - SoftDevice S145
   * - **Multirole**
     - --
     - Supported
   * - **2 Mbps PHY**
     - Supported
     - Supported
   * - **Advertiser**
     - Supported
     - Supported
   * - **Peripheral**
     - Supported
     - Supported
   * - **Scanner**
     - --
     - Supported
   * - **Central**
     - --
     - Supported
   * - **Data Length Extension**
     - Supported
     - Supported
   * - **Advertising Extension**
     - --
     - Supported
   * - **Coded PHY**
     - --
     - --


For more details on supported Bluetooth features, see the SoftDevice Specification documents:

* `S115 SoftDevice Specification`_
* S145 (Specification not available yet)

NFC features support
********************

The following table indicates the software maturity levels of the support for each NFC feature.

.. list-table:: NFC features support
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
     - nRF54LM20A
     - nRF54LS05B
   * - **NFC Type 2 Tag (read-only)**
     - Experimental
     - Experimental
     - Experimental
     - --
     - --
     - --
   * - **NFC Type 4 Tag (read/write)**
     - Experimental
     - Experimental
     - Experimental
     - --
     - --
     - --
   * - **NDEF encoding and decoding**
     - Supported
     - Supported
     - Supported
     - --
     - --
     - --
   * - **NFC Record Type Definition: URI, text**
     - Experimental
     - Experimental
     - Experimental
     - --
     - --
     - --
   * - **NFC Connection Handover to Bluetooth carrier, Static and Negotiated Handover**
     - In development
     - In development
     - In development
     - --
     - --
     - --
   * - **NFC Tag NDEF Exchange Protocol (TNEP)**
     - In development
     - In development
     - In development
     - --
     - --
     - --

nRF Secure Immutable Bootloader
*******************************

The nRF Secure Immutable Bootloader (NSIB) is not supported in |BMshort|.
Instead, Immutable MCUboot serves as the single-stage bootloader in the single-bank DFU solution.

MCUboot bootloader
******************

The following table indicates the software maturity levels of the support for each MCUboot bootloader feature.

.. list-table:: Bootloader and security features
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
     - nRF54LM20A
     - nRF54LS05B
   * - **Immutable MCUboot as part of build**
     - Supported
     - Supported
     - Supported
     - --
     - --
     - --
   * - **Updatable MCUboot as part of build**
     - --
     - --
     - --
     - --
     - --
     - --
   * - **Hardware cryptography acceleration**
     - Supported
     - Supported
     - Supported
     - --
     - --
     - --
   * - **Multiple signature keys**
     - Supported
     - Supported
     - Supported
     - --
     - --
     - --
