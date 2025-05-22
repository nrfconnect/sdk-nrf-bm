.. _install_nrf_bm:

Installing the |BMlong|
#######################

.. contents::
   :local:
   :depth: 2

You can use Visual Studio Code and the `nRF Connect for Visual Studio Code`_ extension to install |BMshort|.

Install prerequisites
*********************

Install the following software tools:

* The latest version of nRF Util, a unified command-line utility for Nordic Semiconductor products.
  Check `operating system versions that support this tool`_ and download the installer - `nRF Util Downloads`_.

  * After downloading the nRF Util executable, move it to a directory that is in the system :envvar:`PATH`.
  * On macOS and Linux, the downloaded file also needs to be given execute permission by typing `chmod +x nrfutil` or by checking the checkbox in the file properties.

  .. important::
     On Windows, nRF Util requires the installation of the `latest Microsoft Visual C++ Redistributable version`_.

* The nRF Util ``device`` command.
  This command allows you to perform various operations on Nordic devices.

   .. code-block:: console

      nrfutil install device

  For the complete list of functionalities offered by this command, type:

   .. code-block:: console

      nrfutil device --help

*   The |jlink_ver| of SEGGER J-Link.
    Download it from the `J-Link Software and Documentation Pack`_ page.

* |VSC|:

  * The latest version of |VSC| for your operating system from the `Visual Studio Code download page`_.
  * In |VSC|, the latest version of the `nRF Connect for VS Code Extension Pack`_.
    The |nRFVSC| comes with its own bundled version of some of the nRF Util commands.

.. _nrf_bm_installing_toolchain:

Install the toolchain
*********************

The |BMshort| toolchain includes tools and modules required to build the samples and applications on top of it.

Use nRF Connect for VS Code to install the toolchain:

.. note::
   These instructions assume that you are using |nRFVSC| version 2025.1.127.
   Newer versions of the extension might feature changes to the user interface.

1. Open the nRF Connect extension in |VSC| by clicking its icon in the :guilabel:`Activity Bar`.
#. In the extension's :guilabel:`Welcome View`, click on :guilabel:`Manage toolchains`.
   The list of actions appears in the |VSC|'s quick pick.
#. Click :guilabel:`Install Toolchain`.
   The list of available stable toolchain versions appears in the |VSC|'s quick pick.
#. Select the toolchain version to install.
   For this release of |BMshort|, use version |ncs_release| of the toolchain.

The toolchain installation starts in the background, as can be seen in the notification that appears.
If this is your first installation of the toolchain, wait for it to finish before moving to the next step of this procedure (getting the code).

When you install the toolchain for the first time, the installed version is automatically selected for your project.

.. _cloning_the_repositories_nrf_bm:

Getting the code
****************

Every |BMlong| release consists of:

* A combination of Git repositories at different versions and revisions, managed together by West.
* An archive containing a source mirror of the Git repositories required to get started with |BMshort|.

.. tabs::

   .. group-tab:: Git

      .. important::
         Make sure that ``git`` is installed on your system before starting this procedure.

      Complete the following steps to clone the |BMshort| repositories.

      1. Open the nRF Connect extension in |VSC| by clicking its icon in the :guilabel:`Activity Bar`.
      #. In the extension's :guilabel:`Welcome View`, click on :guilabel:`Manage SDKs`.
         The list of actions appears in the |VSC|'s quick pick.
      #. Click :guilabel:`Install SDK`.
         The list of available stable SDK versions appears in the |VSC|'s quick pick.
      #. Select the SDK version to install.
         For this release of |BMshort|, use version |ncs_release| of the SDK.

         The SDK installation starts and it can take several minutes.
      #. Open command line and navigate to the SDK installation folder.
         The default location to install the SDK is :file:`C:/ncs/v3.0.1` on Windows, :file:`~/ncs/v3.0.1` on Linux, and :file:`/opt/nordic/ncs/v3.0.1` on macOS.
      #. Clone the `sdk-nrf-bm`_ repository.

         .. tabs::

            .. group-tab:: Windows

               .. code-block:: console

                  cd C:/ncs/v3.0.1
                  git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm

            .. group-tab:: Linux

               .. code-block:: console

                  cd ~/ncs/v3.0.1
                  git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm

            .. group-tab:: macOS

               .. code-block:: console

                  cd /opt/nordic/ncs/v3.0.1
                  git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm


      #. In |VSC|, click :guilabel:`Manage SDKs` -> :guilabel:`Manage West Workspace...` -> :guilabel:`Set West Manifest Repository`.
         From the list that appears, select the ``nrf-bm`` west manifest file.
      #. Then, click :guilabel:`Manage SDKs` -> :guilabel:`Manage West Workspace...` -> :guilabel:`West Update`.
         Your local repositories will be updated.

   .. group-tab:: SDK Archive

      Complete the following steps to get the |BMshort| code using the SDK archive.

      1. Download the archive from the following link:

         https://files.nordicsemi.com/artifactory/ncs-src-mirror/external/sdk-nrf-bm/v0.7.0/src.tar.gz

      #. Extract the archive to the recommended location.

         .. tabs::

            .. group-tab:: Windows

               * Ensure the folder :file:`C:/ncs/v3.0.1` exists.
                 If it does not exist, create it in File Explorer or by running the following command in Command Prompt:

                  .. code-block:: console

                     mkdir C:\ncs\v3.0.1

               * Right-click the downloaded :file:`src.tar.gz` file.
               * Select :guilabel:`Extract All...` and choose :file:`C:/ncs/v3.0.1` as destination.

            .. group-tab:: Linux

               .. code-block:: console

                  mkdir -p ~/ncs/v3.0.1
                  tar -xzf src.tar.gz -C ~/ncs/v3.0.1

            .. group-tab:: macOS

               .. code-block:: console

                  sudo mkdir -p /opt/nordic/ncs/v3.0.1
                  sudo tar -xzf src.tar.gz -C /opt/nordic/ncs/v3.0.1

         .. note::
            The extraction can take several minutes.

      #. Open the nRF Connect extension in |VSC|.

      #. In the extension's :guilabel:`Welcome View`, click on :guilabel:`Manage toolchains` and select :guilabel:`Open terminal profile`.
         The nRF Connect terminal opens with the correct environment.

      #. Navigate to the extracted SDK folder.

         .. tabs::

            .. group-tab:: Windows

               .. code-block:: console

                  cd C:/ncs/v3.0.1

            .. group-tab:: Linux

               .. code-block:: console

                  cd ~/ncs/v3.0.1

            .. group-tab:: macOS

               .. code-block:: console

                  cd /opt/nordic/ncs/v3.0.1

      #. Run the following command to export the Zephyr CMake package:

         .. code-block:: console

            west zephyr-export

      #. In the extension's :guilabel:`Welcome View`, click the refresh icon next to :guilabel:`Manage SDKs`.
         The SDK list will be updated.

Your directory structure should now look similar to this:

.. code-block:: none

   ncs
   ├─── toolchains
   │  └─── <toolchain_version>
   └─── <sdk_version>
      ├─── bootloader
      ├─── modules
      ├─── nrf
      ├─── nrf-bm
      ├─── nrfxlib
      ├─── test
      ├─── tools
      ├─── zephyr

Next steps
**********

You can now proceed to test the :ref:`samples` included in this version of |BMshort|.

The samples can be found in the :file:`nrf-bm/samples` folder, and are divided into two subfolders:

* :file:`bluetooth` for the samples showcasing BLE functionalities using the SoftDevice.
* :file:`peripherals` for the samples showcasing various peripheral functionalities that do not require the SoftDevice.

Each sample documentation contains full information on how to build, flash, and test the respective sample.
