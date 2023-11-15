from mbed_host_tests import BaseHostTest
from mbed_host_tests.host_tests_logger import HtrunLogger

import traceback
import binascii
import sys
import os


class SpiBasicTestHostTest(BaseHostTest):

    """
    Host test for the SPI Basic Test testsuite.
    Handles logging data using the Bus Pirate and verifying certain test results.
    """

    def __init__(self):
        super(SpiBasicTestHostTest, self).__init__()

        self.logger = HtrunLogger('TEST')

        self.mosi_bytes = bytearray()
        self.miso_bytes = bytearray()

    def consume_spi_data(self):
        """
        Consume any new SPI data from the BP and save to class variables.
        Note: Since this test does not use CS, the bus pirate can't tell where one SPI transaction
        starts and the next begins.  So, we just aggregate all SPI data into a single array.
        """
        while True:
            mosi_miso_bytes = self.buspirate_spi.sniff_message()
            if mosi_miso_bytes is None:
                # No more data to read
                break
            else:
                self.mosi_bytes.extend(mosi_miso_bytes[0])
                self.miso_bytes.extend(mosi_miso_bytes[1])

    def _callback_start_recording_spi(self, key: str, value: str, timestamp):
        """
        Called at the start of every test case.  Should start a BP recording of SPI data.
        """

        # Throw out any existing messages
        while self.buspirate_spi.port.in_waiting > 0:
            self.buspirate_spi.sniff_message()

        self.mosi_bytes = bytearray()
        self.miso_bytes = bytearray()

        self.send_kv('start_recording_spi', 'complete')

    def _callback_verify_standard_message(self, key: str, value: str, timestamp):
        """
        Verify that the current recorded SPI data matches the "standard message" defined in the test
        """

        standard_message = b"\x01\x02\x04\x08"
        self.consume_spi_data()

        if self.mosi_bytes == standard_message:
            self.send_kv('verify_standard_message', 'pass')
        else:
            self.logger.prn_err("Incorrect MOSI data.  Expected " + binascii.b2a_hex(standard_message).decode("ASCII"))
            self.send_kv('verify_standard_message', 'fail')

    def _callback_verify_queue_and_abort_test(self, key: str, value: str, timestamp):
        """
        Verify that the current recorded SPI data matches the queueing and abort test
        """

        self.consume_spi_data()

        data_valid = False

        # We should see a block starting with \x01\x02, then less than 30 0 bytes.
        # Then, we should see another \x01\x02 and then 30 0 bytes
        messages = self.mosi_bytes.split(b"\x01")

        if len(messages) == 3: # Note: 0 byte message will be seen at the start due to how split() works
            print("Correct number of messages")
            if messages[1][0] == 0x2 and len(messages[1]) < 31:
                print("First message looks OK")
                if messages[2][0] == 0x2 and len(messages[2]) == 31:
                    print("Second message looks OK")
                    data_valid = True

        if data_valid:
            self.send_kv('verify_queue_and_abort_test', 'pass')
        else:
            self.logger.prn_err("Incorrect MOSI data for queue and abort test")
            self.send_kv('verify_queue_and_abort_test', 'fail')

    def _callback_print_spi_data(self, key: str, value: str, timestamp):
        """
        Called at the end of every test case.  Should print all accumulated BP SPI data
        """

        self.consume_spi_data()

        self.logger.prn_inf("Bus Pirate reads MOSI bytes as: " + binascii.b2a_hex(self.mosi_bytes).decode("ASCII"))
        self.logger.prn_inf("Bus Pirate reads MISO bytes as: " + binascii.b2a_hex(self.miso_bytes).decode("ASCII"))

        self.send_kv('print_spi_data', 'complete')

    def setup(self):

        # Initialize bus pirate.  Fail the test if we can't find it.
        self.buspirate_spi = pyBusPirateLite.SPI(connect=False)
        self.buspirate_spi.serial_debug = False
        try:
            self.buspirate_spi.connect()
            self.buspirate_spi.config = pyBusPirateLite.SPI.CONFIG_DRIVERS_OPEN_DRAIN | \
                                        pyBusPirateLite.SPI.CONFIG_SAMPLE_TIME_MIDDLE | \
                                        pyBusPirateLite.SPI.CONFIG_CLOCK_PHASE_0 | \
                                        pyBusPirateLite.SPI.CONFIG_CLOCK_POLARITY_ACT_LOW
            self.buspirate_spi.enter_sniff_mode(False)

        except Exception:
            traceback.print_exc()
            self.notify_complete(False)

        self.register_callback('start_recording_spi', self._callback_start_recording_spi)
        self.register_callback('verify_standard_message', self._callback_verify_standard_message)
        self.register_callback('verify_queue_and_abort_test', self._callback_verify_queue_and_abort_test)
        self.register_callback('print_spi_data', self._callback_print_spi_data)

        self.logger.prn_inf("SPI Basic Test host test setup complete.")

    def teardown(self):
        self.buspirate_spi.disconnect()