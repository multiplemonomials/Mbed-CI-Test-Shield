## Module to interface with the test shield's internal
## Sigrok-based logic analyzer.
## Handles running the Sigrok command and parsing the results.
import binascii
# Note: This module file cannot be in the host_tests directory, because the test runner iterates through
# and imports all the modules in that directory.  So, if it's in there, it gets imported twice, and really
# bad stuff happens.

import re
import os
import shlex
import subprocess
import time
from typing import List, cast, Optional, Tuple
from dataclasses import dataclass

if "MBED_SIGROK_COMMAND" not in os.environ:
    raise RuntimeError("Must set the MBED_SIGROK_COMMAND environment variable to use the Sigrok logic analyzer tests.")

# common sigrok command -- for all protocols
SIGROK_COMMON_COMMAND = [*shlex.split(os.environ["MBED_SIGROK_COMMAND"]),
                         "--driver", "fx2lafw",  # Set driver to FX2LAFW
                         "--config",
                         # 4 MHz seems to be the best I can get on my PCB before running into a "device only sent x samples" error
                         # Additionally, for decoding messages right at the trigger, we need to change the "capture ratio"
                         # option so that just a few samples are kept from before the trigger.
                         # Details here: https://sigrok.org/bugzilla/show_bug.cgi?id=1657
                         # The hard part was figuring out how to change the capture ratio as there is zero documentation.
                         # It appears that it's a percentage from 0 to 100.
                         "samplerate=4 MHz:captureratio=5"
                         ]

# i2c sigrok command
SIGROK_I2C_COMMAND = [*SIGROK_COMMON_COMMAND,
                      "--protocol-decoders",
                      "i2c:scl=D0:sda=D1:address_format=unshifted",  # Set up I2C decoder
                      "--protocol-decoder-annotations",
                      "i2c=address-read:address-write:data-read:data-write:start:repeat-start:ack:nack:stop",  # Request output of all detectable conditions

                      # Trigger on falling edge of SCL
                      "--triggers",
                      "D0=f",
                      ]


# How long to wait, in seconds, after starting a sigrok recording before we can start the test.
# It would sure be nice if sigrok had some sort of "I'm ready to capture data" printout...
SIGROK_START_DELAY = 0.5 # s


class I2CBusData:
    """
    Empty base class for all I2C .
    Subclasses must define __eq__ and __str__.
    """


class I2CStart(I2CBusData):
    """
    Represents an I2C start condition
    """

    def __str__(self):
        return "Start"

    def __eq__(self, other):
        return isinstance(other, I2CStart)


class I2CRepeatedStart(I2CBusData):
    """
    Represents an I2C repeated start condition
    """

    def __str__(self):
        return "RepeatedStart"

    def __eq__(self, other):
        return isinstance(other, I2CRepeatedStart)


class I2CWriteToAddr(I2CBusData):
    """
    Represents an I2C write to the given (8-bit) address
    """

    def __init__(self, address: int):
        self._address = address

    def __str__(self):
        return f"Wr[0x{self._address:02x}]"

    def __eq__(self, other):
        if isinstance(other, I2CWriteToAddr):
            return other._address == self._address
        return False


class I2CReadFromAddr(I2CBusData):
    """
    Represents an I2C read from the given (8-bit) address
    """

    def __init__(self, address: int):
        self._address = address

    def __str__(self):
        return f"Rd[0x{self._address:02x}]"

    def __eq__(self, other):
        if isinstance(other, I2CReadFromAddr):
            return other._address == self._address
        return False


class I2CDataByte(I2CBusData):
    """
    Represents an I2C data byte on the bus.
    """

    def __init__(self, data: int):
        self._data = data

    def __str__(self):
        return f"0x{self._data:02x}"

    def __eq__(self, other):
        if isinstance(other, I2CDataByte):
            return other._data == self._data
        return False


class I2CAck(I2CBusData):
    """
    Represents an acknowledge on the I2C bus
    """

    def __str__(self):
        return "Ack"

    def __eq__(self, other):
        return isinstance(other, I2CAck)

class I2CNack(I2CBusData):
    """
    Represents a not-acknowledge on the I2C bus
    """

    def __str__(self):
        return "Nack"

    def __eq__(self, other):
        return isinstance(other, I2CNack)


class I2CStop(I2CBusData):
    """
    Represents a stop event on the I2C bus
    """

    def __str__(self):
        return "Stop"

    def __eq__(self, other):
        return isinstance(other, I2CStop)


# Regexes for parsing the sigrok I2C output
SR_I2C_REPEATED_START = re.compile(r'i2c-1: Start repeat')
SR_I2C_START = re.compile(r'i2c-1: Start')
SR_I2C_WRITE_TO_ADDR = re.compile(r'i2c-1: Address write: (..)')
SR_I2C_READ_FROM_ADDR = re.compile(r'i2c-1: Address read: (..)')
SR_I2C_DATA_BYTE = re.compile(r'i2c-1: Data [^ ]+: (..)') # matches "Data read" or "Data write"
SR_I2C_ACK = re.compile(r'i2c-1: ACK')
SR_I2C_NACK = re.compile(r'i2c-1: NACK')
SR_I2C_STOP = re.compile(r'i2c-1: Stop')


class SigrokI2CRecorder():
    def record(self, record_time: float):
        """
        Starts recording I2C data from the logic analyzer.
        :param record_time: Time after the first clock edge to record data for
        """
        # Run sigrok for the specified amount of milliseconds
        command = [*SIGROK_I2C_COMMAND, "--time", str(round(record_time * 1000))]
        #print("Executing: " + " ".join(command))
        self._sigrok_process = subprocess.Popen(command, text=True, stdout = subprocess.PIPE)
        time.sleep(SIGROK_START_DELAY)

    def get_result(self) -> List[I2CBusData]:
        """
        Get the data that was recorded
        :return: Data recorded (list of I2CBusData subclasses)
        """

        self._sigrok_process.wait(5)

        if self._sigrok_process.returncode != 0:
            raise RuntimeError("Sigrok failed!")

        i2c_transaction: List[I2CBusData] = []

        # Parse output
        for line in self._sigrok_process.communicate()[0].split("\n"):
            # Note: Must check repeated start first because repeated start is a substring of start,
            # so the start regex will match it as well.
            if SR_I2C_REPEATED_START.match(line):
                i2c_transaction.append(I2CRepeatedStart())
            elif SR_I2C_START.match(line):
                i2c_transaction.append(I2CStart())
            elif SR_I2C_WRITE_TO_ADDR.match(line):
                write_address = int(SR_I2C_WRITE_TO_ADDR.match(line).group(1), 16)
                i2c_transaction.append(I2CWriteToAddr(write_address))
            elif SR_I2C_READ_FROM_ADDR.match(line):
                read_address = int(SR_I2C_READ_FROM_ADDR.match(line).group(1), 16)
                i2c_transaction.append(I2CReadFromAddr(read_address))
            elif SR_I2C_DATA_BYTE.match(line):
                data = int(SR_I2C_DATA_BYTE.match(line).group(1), 16)
                i2c_transaction.append(I2CDataByte(data))
            elif SR_I2C_ACK.match(line):
                i2c_transaction.append(I2CAck())
            elif SR_I2C_NACK.match(line):
                i2c_transaction.append(I2CNack())
            elif SR_I2C_STOP.match(line):
                i2c_transaction.append(I2CStop())
            elif line == "i2c-1: Read" or line == "i2c-1: Write" or len(line) == 0:
                # we can ignore these ones
                pass
            else:
                print(f"Warning: Unparsed Sigrok output: '{line}'")

        return i2c_transaction


@dataclass
class SPITransaction():
    # Bytes seen on MOSI
    mosi_bytes: bytes

    # Bytes seen on MISO.  Note: Always has the same length as mosi_bytes
    miso_bytes: bytes

    def __init__(self, mosi_bytes, miso_bytes):
        """
        Construct an SPITransaction.  Accepts values for the mosi and miso bytes that are convertible
        to bytes (e.g. bytes, bytearray, or an iterable of integers).
        """
        self.mosi_bytes = bytes(mosi_bytes)
        self.miso_bytes = bytes(miso_bytes)
        if len(mosi_bytes) != len(miso_bytes):
            raise ValueError("MOSI and MISO bytes are not the same length!")

    def __str__(self):
        return f"[mosi: {binascii.b2a_hex(self.mosi_bytes)}, miso: {binascii.b2a_hex(self.miso_bytes)}]"

    def __cmp__(self, other):
        if not isinstance(other, SPITransaction):
            return False

        return other.mosi_bytes == self.mosi_bytes and other.miso_bytes == self.miso_bytes

# Regex for one SPI data byte
SR_SPI_DATA_BYTE = re.compile(r'spi-1: ([0-9A-F][0-9A-F])')

# Regex for multiple data bytes in one transaction
SR_SPI_DATA_BYTES = re.compile(r'spi-1: ([0-9A-F ]+)')


class SigrokSPIRecorder():

    def record(self, cs_pin: Optional[str], record_time: float):
        """
        Starts recording SPI data from the logic analyzer.
        :param cs_pin: Logic analyzer pin to use for chip select.  e.g. "D3" or "D4".  May be set to None
           to not use the CS line and record all traffic.
        :param record_time: Time after the first clock edge to record data for
        """

        self._has_cs_pin = cs_pin is not None

        # spi sigrok command
        sigrok_spi_command = [*SIGROK_COMMON_COMMAND,

                      # Set up SPI decoder.
                      # Note that for now we always use mode 0 and a word size of 8, but that can be changed later.
                      "--protocol-decoders",
                      f"spi:clk=D0:mosi=D1:miso=D2{':cs=' + cs_pin if self._has_cs_pin else ''}:cpol=0:cpha=0:wordsize=8",

                      # Run sigrok for the specified amount of milliseconds
                      "--time", str(round(record_time * 1000))
                      ]
        if self._has_cs_pin:
            # Trigger on falling edge of CS
            sigrok_spi_command.append("--triggers")
            sigrok_spi_command.append(f"{cs_pin}=f")

            # Output complete transactions
            sigrok_spi_command.append("--protocol-decoder-annotations")
            sigrok_spi_command.append("spi=mosi-transfer:miso-transfer")
        else:
            # Trigger on any edge of clock
            sigrok_spi_command.append("--triggers")
            sigrok_spi_command.append("D0=e")

            # The decoder has no transaction information without CS.
            # So, we have to just get the raw bytes
            sigrok_spi_command.append("--protocol-decoder-annotations")
            sigrok_spi_command.append("spi=mosi-data:miso-data")

        self._sigrok_process = subprocess.Popen(sigrok_spi_command, text=True, stdout = subprocess.PIPE)
        time.sleep(SIGROK_START_DELAY)

    def get_result(self) -> List[SPITransaction]:
        """
        Get the SPI data recorded by the logic analyzer.
        :return: List of SPI transactions observed.  Note that if CS was not provided, every byte will
            be considered as part of a single transaction.
        """

        self._sigrok_process.wait(5)

        if self._sigrok_process.returncode != 0:
            raise RuntimeError("Sigrok failed!")

        sigrok_output = self._sigrok_process.communicate()[0].split("\n")

        if self._has_cs_pin:

            # If we have a CS pin then we will have multiple transactions to handle
            spi_data : List[SPITransaction] = []

            # Bytes from the previous line if this is an even line
            previous_line_data: Optional[List[int]] = None

            for line in sigrok_output:

                # Skip empty lines
                if line == "":
                    continue

                match_info = SR_SPI_DATA_BYTES.match(line)
                if not match_info:
                    print(f"Warning: Unparsed Sigrok output: '{line}'")
                    continue

                # Parse list of hex bytes
                byte_strings = match_info.group(1).split("")
                byte_values = [int(byte_string, 16) for byte_string in byte_strings]

                if previous_line_data is None:
                    previous_line_data = byte_values
                else:
                    # It appears that sigrok always alternates MISO, then MOSI lines in its CLI output.
                    # This is not documented anywhere, so I had to test it on hardware.
                    spi_data.append(SPITransaction(mosi_bytes=byte_values, miso_bytes=previous_line_data))
                    previous_line_data = None

            return spi_data

        else:

            # When we don't have a CS pin, we will get a bunch of lines with one data byte per line.
            mosi_bytes = []
            miso_bytes = []

            # It appears that sigrok always alternates MISO, then MOSI lines in its CLI output.
            # This is not documented anywhere, so I had to test it on hardware.
            next_line_is_miso = True

            for line in sigrok_output:

                # Skip empty lines
                if line == "":
                    continue

                match_info = SR_SPI_DATA_BYTES.match(line)
                if not match_info:
                    print(f"Warning: Unparsed Sigrok output: '{line}'")
                    continue

                byte_value = int(match_info.group(1), 16)

                if next_line_is_miso:
                    miso_bytes.append(byte_value)
                    next_line_is_miso = False
                else:
                    mosi_bytes.append(byte_value)
                    next_line_is_miso = True

            return [SPITransaction(miso_bytes=miso_bytes, mosi_bytes=mosi_bytes)]