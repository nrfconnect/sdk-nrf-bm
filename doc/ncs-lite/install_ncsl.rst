.. _install_ncsl:

Installing the |NCSL|
#####################

.. contents::
   :local:
   :depth: 2

You can use the command line and `nRF Util`_ to install |NCSL|.



Install prerequisites
*********************

Install the following software tools.


* The latest version of nRF Util, a unified command-line utility for Nordic Semiconductor products.
  Check `operating system versions that support this tool`_ and download the installer - `nRF Util Downloads`_.

  .. note::
     After downloading the nRF Util executable, move it to a directory that is in the system :envvar:`PATH`.
     On macOS and Linux, the downloaded file also needs to be given execute permission by typing `chmod +x nrfutil` or by checking the checkbox in the file properties.

* The latest version of the `nRF Command Line Tools`_ package.
  Check :ref:`operating system versions that support this tool` and download the installer - `nRF Command Line Tools Downloads`_.

  .. note::
     After downloading and installing the tools, add nrfjprog to the system :envvar:`PATH` in the environment variables.

* The |jlink_ver| of SEGGER J-Link.
  Download it from the `J-Link Software and Documentation Pack`_ page.
  On Windows, `install it manually together with SEGGER USB Driver for J-Link <nRF Util prerequisites_>`_.

* Additionally, for Windows users: SEGGER USB Driver for J-Link, required for support of older Nordic Semiconductor devices in nRF Util.
  For information on how to install the USB Driver, see the `nRF Util prerequisites`_ documentation.

* Additionally, for Linux users: the `nrf-udev`_ module with udev rules required to access USB ports on Nordic Semiconductor devices and program the firmware.

.. _ncsl_installing_toolchain:

Install the |NCSL| toolchain
****************************

The |NCSL| toolchain includes tools and modules required to build the samples and applications on top of it.

Use the command line to install the toolchain:

1. Open a terminal window.
#. Run the following command to install the nRF Util ``toolchain-manager`` command:

   .. code-block:: console

      nrfutil install toolchain-manager

#. Run the following command to list the available installations:

   .. code-block:: console

      nrfutil toolchain-manager search

   The versions from this list correspond to the |NCS| versions and will be *version* in the following step.
#. Run the following command to install the toolchain version for the SDK version of your choice:

   .. parsed-literal::
      :class: highlight

      nrfutil toolchain-manager install --ncs-version *version*

   For example:

   .. parsed-literal::
      :class: highlight

      nrfutil toolchain-manager install --ncs-version |ncs_release|

   This example command installs the toolchain required for the |NCS| |ncs_release|, which is also used by |NCSL| |release|.

The ``toolchain-manager`` command installs the toolchain by default at :file:`C:/ncs/toolchains` on Windows and at :file:`~/ncs/toolchains` on Linux.
These can be modified, as explained in the `Toolchain Manager command`_ documentation.
To check the current configuration setting, use the ``nrfutil toolchain-manager config --show`` command.
On macOS, :file:`/opt/nordic/ncs/toolchains` is used and no other location is allowed.

If you have received a custom URL for installing the toolchain, you can use the following command to set it as default, replacing the respective parameter:

.. parsed-literal::
   :class: highlight

   nrfutil toolchain-manager config --set toolchain-index=\ *custom_toolchain_URL*

If you have received a custom bundle ID for installing a specific toolchain version, you can use the following commands to provide it, replacing the respective parameter:

.. parsed-literal::
   :class: highlight

   nrfutil toolchain-manager install --bundle-id *custom_bundle_ID*

To read more about ``toolchain-manager`` commands, use the ``nrfutil toolchain-manager --help`` command.

With the default location to install the toolchain (:file:`C:/ncs/toolchains` on Windows, :file:`~/ncs/toolchains/` on Linux, and the non-modifiable :file:`/opt/nordic/ncs/toolchains/` on macOS), your directory structure now looks similar to this:

.. code-block:: none

   ncs
   └─── toolchains
      └─── <toolchain-installation>

In this simplified structure preview, *<toolchain-installation>* corresponds to the version name you installed (most commonly, a SHA).

.. _cloning_the_repositories_ncsl:

Getting the |NCSL| code
***********************

To clone the repositories, complete the following steps:

1. On the command line, open the directory :file:`ncs`.
   By default, this is one level up from the location where you installed the toolchain.
   This directory will hold all |NCS| repositories.

#. Start the toolchain environment for your operating system using the following command:

   .. tabs::

      .. tab:: Windows

         .. code-block:: console

            nrfutil toolchain-manager launch --terminal

      .. tab:: Linux

         .. code-block:: console

            nrfutil toolchain-manager launch --shell

      .. tab:: macOS

         .. code-block:: console

            nrfutil toolchain-manager launch --shell

#. Initialize west with the revision of the |NCSL| that you want to check out:

   .. parsed-literal::
      :class: highlight

      west init -m https://github.com/nrfconnect/sdk-ncs-lite --mr v0.1.0

   This will clone the manifest repository `sdk-ncs-lite`_ into :file:`ncs-lite`.

   Initializing west with a specific revision of the repository, such as ``v0.1.0`` does not lock your repositories to this version.
   Checking out a different branch or tag in the `sdk-ncs-lite`_ repository and running ``west update`` changes the version of the |NCSL| that you work with.

#. Enter the :file:`ncs-lite` subdirectory and run the following command to clone the project repositories:

   .. code-block:: console

      west update

   Depending on your connection, this might take some time.

#. Export a :ref:`Zephyr CMake package <zephyr:cmake_pkg>`.
   This allows CMake to automatically load the boilerplate code required for building |NCSL| applications:

   .. code-block:: console

      west zephyr-export


With the default location to install the toolchain (see the previous step) and the default location to install the SDK (:file:`C:/ncs` on Windows, :file:`~/ncs/` on Linux, and :file:`/opt/nordic/ncs/` on macOS), your directory structure now looks similar to this:

.. code-block:: none

   ncs
   ├─── toolchains
   │  └─── <toolchain-installation>
   └─── <west-workspace>
      ├─── .west
      ├─── bootloader
      ├─── modules
      ├─── nrf
      ├─── nrfxlib
      ├─── zephyr
      └─── ...

In this simplified structure preview, *<toolchain-installation>* corresponds to the toolchain version and *<west-workspace>* corresponds to the SDK version name.
There are also additional directories, and the structure might change over time, for example if you later change the state of development to a different revision.
The full set of repositories and directories is defined in the west manifest file.

.. _build_environment_cli:

Set up the command-line build environment
*****************************************

In addition to the steps mentioned above, if you want to build and program your application from the command line, you have to set up your command-line build environment by defining the required environment variables every time you open a new command-line or terminal window.

Define the required environment variables as follows, depending on your operating system:

.. tabs::

   .. tab:: Windows

      Complete the following steps:

      1. Navigate to the :file:`ncs` directory.
      #. Open the directory for your |NCS| version.
      #. Run the following command in a terminal window:

         .. code-block:: console

            zephyr\zephyr-env.cmd

      If you need to define additional environment variables, create the file :file:`%userprofile%/zephyrrc.cmd` and add the variables there.
      This file is loaded automatically when you run the above command.

   .. tab:: Linux

      Complete the following steps:

      1. Navigate to the :file:`ncs` directory.
      #. Open the directory for your |NCS| version.
      #. Run the following command in a terminal window:

         .. code-block:: console

            source zephyr/zephyr-env.sh

      If you need to define additional environment variables, create the file :file:`~/.zephyrrc` and add the variables there.
      This file is loaded automatically when you run the above command.

   .. tab:: macOS

      Complete the following steps:

      1. Navigate to the :file:`ncs` directory.
      #. Open the directory for your |NCS| version.
      #. Run the following command in a terminal window:

         .. code-block:: console

            source zephyr/zephyr-env.sh

      If you need to define additional environment variables, create the file :file:`~/.zephyrrc` and add the variables there.
      This file is loaded automatically when you run the above command.
