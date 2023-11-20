## Module to interface with the test shield's internal
## Sigrok-based logic analyzer.
## Handles running the Sigrok command and parsing the results.

import re
import os
import shlex
import subprocess
import time
from typing import List, cast

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
        return "StartRepeated"

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
SR_I2C_START = re.compile(r'i2c-1: Start')
SR_I2C_REPEATED_START = re.compile(r'i2c-1: Start repeat')
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
        time.sleep(0.5)

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
            if SR_I2C_START.match(line):
                i2c_transaction.append(I2CStart())
            elif SR_I2C_REPEATED_START.match(line):
                i2c_transaction.append(I2CRepeatedStart())
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