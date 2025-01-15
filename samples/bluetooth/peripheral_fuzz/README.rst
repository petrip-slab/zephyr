.. zephyr:code-sample:: ble_peripheral_fuzz
   :name: Heart-rate_fuzz Monitor (Peripheral)
   :relevant-api: bt_hrs

   Expose a Heart Rate (HR) GATT Service for fuzz testing purpose.

Overview
********

This application is meant for fuzz testing with Black Duck + Fuzz Box combination

Similar to the :zephyr:code-sample:`ble_peripheral` sample, except that this
application specifically exposes the HR (Heart Rate) GATT Service. Also added handlers
needed by the tester.

Requirements
************

* BlueZ running on the host, or
* A board with Bluetooth LE support

Building and Running
********************

This sample can be found under :zephyr_file:`samples/bluetooth/peripheral_fuzz` in the
Zephyr tree.

See :zephyr:code-sample-category:`bluetooth` samples for details.
