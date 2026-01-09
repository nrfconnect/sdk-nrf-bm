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

.. note::
   A dedicated |BMshort| software maturity page will be available in upcoming releases.


nRF Devices in scope
********************

The following overview list which devices are in scope of the |BMshort|.

Supported is stated when we have one or more samples/ components demonstrated on the device with software maturity set to Supported.

.. list-table:: nRF54L Series
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
     - --

.. note::
    Devices not listed should be seen as not in scope.


Protocol supported
******************
The following table indicates the software maturity levels of the support for each protocol

.. list-table:: nRF54L Series
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
   * - **Bluetooth**
     - Supported
     - Supported
     - Supported
     -
   * - **NFC**
     - Experimental
     - Experimental
     - Experimental
     -
   * - **ESB**
     -
     -
     -
     -

.. note::
    Protocol not listed is seen as out of scope for |BMshort|.

Bluetooth feature supported
***************************

For details on Bluetooth features supported see SoftDevice Specification
   * S115  (https://docs.nordicsemi.com/bundle/sds_s115/page/SDS/s1xx/s115.html)
   * S145  (TBD)


.. list-table:: nRF54L Series
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
   * - **SoftDevice S115**
     - Supported
     - Supported
     - Supported
     - --
   * - **SoftDevice S145**
     - Experimental
     - Experimental
     - Experimental
     - --

NFC features supported
**********************
The following table indicates the software maturity levels of the support for each NFC feature.

.. list-table:: nRF54L Series
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
   * - **NFC Type 2 Tag (read-only)**
     - Experimental
     - Experimental
     - Experimental
     - --
   * - **NFC Type 4 Tag (read/ write)**
     - Production
     - Production
     - Production
     - --
   * - **NFC Reader/Writer (polling device)**
     - --
     - --
     - --
     - --
   * - **NFC ISO-DEP protocol (ISO/IEC 14443-4)**
     - --
     - --
     - --
     - --
   * - **NDEF encoding/decoding**
     - Production
     - Production
     - Production
     - --
   * - **NFC Record Type Definitions: URI, Text, Connection Handover**
     - Experimental
     - Experimental
     - Experimental
     - --
   * - **NFC Connection Handover to Bluetooth carrier, Static and Negotiated Handover**
     - :sup:`1`
     - :sup:`1`
     - :sup:`1`
     - --
   * - **NFC Tag NDEF Exchange Protocol (TNEP)**
     - :sup:`1`
     - :sup:`1`
     - :sup:`1`
     - --

| [1]: Feature is in development, required for OOB pairing.


nRF Secure Immutable Bootloader
*******************************
nRF Secure Immutable Bootloader is not supported in |BMshort|.
We are using Imutable MCUboot as the single stage bootloader solution in the Single Bank DFU solution.


MCUboot Bootloader
******************
The following table indicates the software maturity levels of the support for each MCUboot bootloader feature.

.. list-table:: nRF54L Series
   :header-rows: 1
   :align: center
   :widths: auto

   * -
     - nRF54L15
     - nRF54L10
     - nRF54L05
     - nRF54LV10
   * - **Immutable MCUboot as part of build**
     - --
     - --
     - --
     - --
   * - **Updateable MCUboot as part of build**
     - Supported
     - Supported
     - Supported
     - --
   * - **Hardware cryptography acceleration**
     - Supported
     - Supported
     - Supported
     - --
   * - **Multiple signature keys**
     - Supported
     - Supported
     - Supported
     - --
