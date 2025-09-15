.. _ug_bootloader_keys:

Bootloader keys
###############

When MCUboot is used in a project, by default it uses a dummy ed25519 signing key.
This key should only be used for development purposes.

For testing and production use cases, unique signing keys must be generated and kept secure (one key per project) to ensure the integrity of firmware update security.

Signature type
--------------

MCUboot in |BMshort| supports the following signature types:

+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| Type       | Description                                                          | Sysbuild Kconfig                                                            |
+============+======================================================================+=============================================================================+
| None       | No signature verification (insecure)                                 | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_NONE`       |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| RSA        | RSA-2048 or RSA-3072 signature                                       | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_RSA`        |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| ECDSA-P256 | Elliptic curve digital signature with curve P-256                    | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_ECDSA_P256` |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+
| ed25519    | Edwards curve digital signature using ed25519 (recommended, default) | :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_TYPE_ED25519`    |
+------------+----------------------------------------------------------------------+-----------------------------------------------------------------------------+

.. _ug_bootloader_keys_generating:

Generating a key
----------------

See `Image tool`_ documentation for details on the ``imgtool`` which includes details on how to generate a signing key.

.. _ug_bootloader_keys_using:

Using a key in a project
------------------------

Once a key has been generated, it can be used in a project by setting the :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_KEY_FILE` sysbuild Kconfig option to the absolute path of the generated ``.pem`` key file.

.. _ug_bootloader_kmu:

KMU (Key Management Unit)
*************************

The nRF54L series of SoCs contain a KMU - key management unit, this on-die peripheral can be used by CRACEN to securely store and use keys without allowing the contents to be read out.
In order to boot images when the KMU feature is enabled, the MCUboot singing key must be programmed to the KMU prior to loading the firmware or the device will be unable to boot.
This feature can be enabled with :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_SIGNATURE_USING_KMU`, another sysbuild Kconfig is used to determine if the public key file should be automatically programmed to boards when ``west flash`` is used with the ``--erase`` or ``--recover`` arguments, when :kconfig:option:`SB_CONFIG_BM_BOOTLOADER_MCUBOOT_GENERATE_DEFAULT_KMU_KEYFILE` is enabled (it is enabled by default) then this process is enabled and should be the first command used after building the project to set the board up for development.
