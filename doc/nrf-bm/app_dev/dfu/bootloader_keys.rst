.. _ug_bootloader_keys:

Bootloader keys
###############

When MCUboot is used in a project, by default it uses a dummy ED25519 signing key.
This key should only be used for development purposes.

For testing and production use cases, unique signing keys must be generated and kept secure (one key per project) to ensure the integrity of firmware update security.

Signature type
**************

MCUboot in |BMshort| allow a few signatures types.
The ED25519 signature type is recommended as supported for nRF54L Series devices with cryptographic hardware support (CRACEN and KMU).
It is recommended to use the pure version of the ED25519 signature (:kconfig:option:`SB_CONFIG_BM_BOOT_IMG_HASH_ALG_PURE`).
The rest of the signature types are for evaluation purpose only and are inherited from the MCUboot project.

The available signature types are listed in the following table:

+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| Type       | Description                                                          | Sysbuild Kconfig                                                            |
+============+======================================================================+=============================================================================+
| None       | No signature verification (insecure, for development only)           | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_NONE`       |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| RSA        | RSA-2048 or RSA-3072 signature                                       | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_RSA`        |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| ECDSA-P256 | Elliptic curve digital signature with curve P-256                    | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_ECDSA_P256` |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| ed25519    | Edwards curve digital signature using ed25519 (recommended, default) | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_ED25519`    |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+

.. _ug_bootloader_keys_generating:

Generating a key
****************

See `Image tool`_ documentation for details on the ``imgtool`` which includes details on how to generate a signing key.

.. _ug_bootloader_keys_using:

Using a key in a project
************************

Once a key has been generated, it can be used in a project by setting the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE` sysbuild Kconfig option to the absolute path of the generated ``.pem`` key file.
For most of the samples, you must create a :file:`sysbuild.conf` file in your application directory and add the sysbuild configuration here.
You can also provide the sysbuild Kconfig option during compilation.

.. _ug_bootloader_kmu:

KMU (Key Management Unit)
*************************

Most nRF54L Series devices include the Key Management Unit (KMU) hardware peripheral.
This on-die module is designed to work with the CRACEN hardware accelerator, enabling secure storage and usage of cryptographic keys.
It also prevents the keys from being externally read, thus enhancing security.

Firmware booting with KMU
=========================

To boot an image that uses the KMU feature, you must program the MCUboot signing key into the KMU before loading the firmware.
If you do not perform this step, the device will not boot.
You can enable this setup through the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_USING_KMU` Kconfig option.

.. _ug_bootloader_kmu_autokeys:

Automatic key programming
=========================

The configuration option :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_GENERATE_DEFAULT_KMU_KEYFILE`, enabled by default, automates the programming of the public key file to the board.
In |nRFVSC|, this automatically happens when using the :guilabel:`Erase and Flash to Board` option.
If using ``west flash`` on the command line, this automatically happens when using the ``--erase`` or ``--recover`` arguments.
It is essential to run this command first after building the project to prepare the board for development.

For more information about the KMU, see `Working with the KMU and CRACEN`_ and `Provisioning the KMU`_.

Runtime revocation
==================

.. note::
   The support for this feature is currently experimental.

MCUboot can invalidate image verification keys through the ``CONFIG_BOOT_KEYS_REVOCATION`` Kconfig option.
Enable this option during the MCUboot build process if there is a risk that images signed with a compromised key might contain critical vulnerabilities.
The revocation of keys is triggered when both the firmware loader and SoftDevice are using a newer key.

The number of available key slots is set by the ``CONFIG_BOOT_SIGNATURE_KMU_SLOTS`` Kconfig option.
These slots have to be properly provisioned.
For more information refer to `Provisioning the KMU`_.

.. caution::
   You must enable the ``CONFIG_BOOT_KEYS_REVOCATION`` Kconfig option when creating your project.
   If you have not activated this option initially, you cannot enable it later.
   Without the option, this functionality is unavailable and will potentially expose your project to security issues.

A valid signature verification must precede any key invalidation.
The last remaining key cannot be invalidated.

To enable the Kconfig options for the MCUboot image, edit the :file:`sysbuild/mcuboot.conf` file in your application directory.
For most of the samples, you must create the :file:`sysbuild` folder and the :file:`sysbuild/mcuboot.conf` file.
This file applies to the MCUboot image only and is edited the same way as the :file:`prj.conf` file is used for your application image.
