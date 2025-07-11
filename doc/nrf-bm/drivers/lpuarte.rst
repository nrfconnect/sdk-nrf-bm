.. _driver_lpuarte:

Low Power UART with EasyDMA (LPUARTE)
#####################################

The Low Power UART with EasyDMA (LPUARTE) driver implements the standard UART with EasyDMA and two extra control lines for low power capabilities.
The control lines allow for disabling the UART receiver during the idle period.
This results in low power consumption, as you can shut down the high-frequency clock when UART is in idle state.

Control protocol
****************

You can use the protocol for duplex transmission between two devices.

The control protocol uses two additional lines alongside the standard TX and RX lines:

* the *Request* line (REQ),
* the *Ready* line (RDY).

The REQ line of the first device is connected with the RDY line of the second device.
It is configured as an output and set to low.

The RDY line is configured as input without pull-up and the interrupt is configured to detect the transition from low to high state.

Implementation
**************

The driver implements the control protocol in the following way:

#. The transmitter initiates the transmission by calling :c:func:`bm_lpuarte_tx`.
#. The driver reconfigures the REQ line to input with pull-up and enables the detection of high to low transition.
#. The line is set to high due to the pull-up register, and that triggers an interrupt on the RDY line on the receiver device.
#. On that interrupt, the driver starts the UART receiver.
#. When the receiver is ready, the driver reconfigures the RDY line to output low.
#. Then, the driver reconfigures the RDY line to input with pull-up and enables the interrupt on detection of high to low transition.
#. This sequence results in a short pulse on the line, as the line goes low and high.
#. The initiator detects the pulse and starts the standard UART transfer.
#. When the transfer is completed, the transmitter driver reconfigures the REQ line to the initial state: output set to low.
   This results in a line state change.
#. As the line goes low, the receiver detects the change.
   This indicates that the UARTE receiver can be stopped.

Once the receiver acknowledges the transfer, it must be capable of receiving the whole transfer until the REQ line goes down.

This requirement can be fulfilled in two ways:

* By providing a buffer of the size of the maximum packet length.
* By continuously responding to the RX buffer request event.
  The latency of the event handling must be taken into account in that case.
  For example, a flash page erase on some devices might have a significant impact.

Sample usage
************

See the :ref:`bm_lpuarte_sample` sample for the implementation of this driver.
