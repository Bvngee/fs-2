#include "BmsThread.h"

#include "LTC681xBus.h"
#include "LTC681xCommand.h"
#include "ThisThread.h"
#include <cstdint>
#include <cstdio>

#include "EnergusTempSensor.h"

BMSThread::BMSThread(LTC681xBus &bus, unsigned int frequency,
                     std::vector<Queue<BmsEvent, mailboxSize> *> mailboxes)
    : m_bus(bus), mailboxes(mailboxes) {
  m_delay = 1000 / frequency;
  for (int i = 0; i < BMS_BANK_COUNT; i++) {
    m_chips.push_back(LTC6811(bus, i));
  }
  for (int i = 0; i < BMS_BANK_COUNT; i++) {
    // m_chips[i].getConfig().gpio5 = LTC6811::GPIOOutputState::kLow;
    // m_chips[i].getConfig().gpio4 = LTC6811::GPIOOutputState::kPassive;

    m_chips[i].updateConfig();
  }
}

void BMSThread::threadWorker() {
  // Perform self tests

  // Cell Voltage self test
  m_bus.WakeupBus();
  printf("wakeup1\n");
  m_bus.SendCommand(LTC681xBus::BuildBroadcastBusCommand(
      StartSelfTestCellVoltage(AdcMode::k7k, SelfTestMode::kSelfTest1)));
  printf("Send Command\n");
  ThisThread::sleep_for(4ms);
  m_bus.WakeupBus();
  printf("wakeup2\n");
  for (int i = 0; i < BMS_BANK_COUNT; i++) {
    uint16_t rawVoltages[12];
    uint16_t rawTemps[12];

    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupA(), i),
            (uint8_t *)rawVoltages) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageA\n");
    }
    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupB(), i),
            (uint8_t *)rawVoltages + 6) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageB\n");
    }
    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupC(), i),
            (uint8_t *)rawVoltages + 12) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageC\n");
    }
    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupD(), i),
            (uint8_t *)rawVoltages + 18) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageD\n");
    }

    printf("beep\n");

    for (int j = 0; j < 12; j++) {
      printf("AXST %2d: %4x\n", j, rawVoltages[i]);
    }
  }

  // Cell GPIO self test
  m_bus.WakeupBus();
  m_bus.SendCommand(LTC681xBus::BuildBroadcastBusCommand(
      StartSelfTestGpio(AdcMode::k7k, SelfTestMode::kSelfTest1)));
  ThisThread::sleep_for(4ms);
  m_bus.WakeupBus();
  for (int i = 0; i < BMS_BANK_COUNT; i++) {
    uint16_t rawVoltages[12];
    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupA(), i),
            (uint8_t *)rawVoltages) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageA\n");
    }
    if (m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupB(), i),
            (uint8_t *)rawVoltages + 6) != LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. SelfTestVoltageB\n");
    }

    for (int j = 0; j < 12; j++) {
      printf("CVST %2d: %4x\n", j, rawVoltages[i]);
    }
  }

  printf("SELF TEST DONE \n");

  std::array<uint16_t, BMS_BANK_COUNT * BMS_BANK_CELL_COUNT> allVoltages;
  std::array<int8_t, BMS_BANK_COUNT * BMS_BANK_TEMP_COUNT> allTemps;
  while (true) {
    printf("\n \n");
    m_bus.WakeupBus();

    // Set all status lights high
    // TODO: This should be in some sort of config class
    
    for (int i = 0; i < BMS_BANK_COUNT; i++) {
        LTC6811::Configuration& config = m_chips[i].getConfig();
        config.gpio5 = LTC6811::GPIOOutputState::kLow;
        m_chips[i].updateConfig();
    }
    

    // Start ADC on all chips
    auto startAdcCmd =
        StartCellVoltageADC(AdcMode::k7k, false, CellSelection::kAll);
    if (m_bus.SendCommand(LTC681xBus::BuildBroadcastBusCommand(startAdcCmd)) !=
        LTC681xBus::LTC681xBusStatus::Ok) {
      printf("Things are not okay. StartADC\n");
    }

    // Read back values from all chips
    for (int i = 0; i < BMS_BANK_COUNT; i++) {
      if (m_bus.PollAdcCompletion(
              LTC681xBus::BuildAddressedBusCommand(PollADCStatus(), 0)) ==
          LTC681xBus::LTC681xBusStatus::PollTimeout) {
        printf("Poll timeout.\n");
      } else {
        printf("Poll OK.\n");
      }

      uint16_t rawVoltages[12];

      if (m_bus.SendReadCommand(
              LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupA(), i),
              (uint8_t *)rawVoltages) != LTC681xBus::LTC681xBusStatus::Ok) {
        printf("Things are not okay. VoltageA\n");
      }
      if (m_bus.SendReadCommand(
              LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupB(), i),
              (uint8_t *)rawVoltages + 6) != LTC681xBus::LTC681xBusStatus::Ok) {
        printf("Things are not okay. VoltageB\n");
      }
      if (m_bus.SendReadCommand(
              LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupC(), i),
              (uint8_t *)rawVoltages + 12) !=
          LTC681xBus::LTC681xBusStatus::Ok) {
        printf("Things are not okay. VoltageC\n");
      }
      if (m_bus.SendReadCommand(
              LTC681xBus::BuildAddressedBusCommand(ReadCellVoltageGroupD(), i),
              (uint8_t *)rawVoltages + 18) !=
          LTC681xBus::LTC681xBusStatus::Ok) {
        printf("Things are not okay. VoltageD\n");
      }

      // MUX get temp from each cell
      for (uint8_t j = 0; j < BMS_BANK_CELL_COUNT; j++) {
        uint8_t muxSelect[6] = {
            static_cast<uint8_t>(
                0b01000000 |
                ((j & 0b111) << 3)), // left most bit controls LED :/ (0 is on)
            0x00,
            0x00,
            0x00,
            0x00,
            0x00};

        m_bus.SendDataCommand(
            LTC681xBus::BuildAddressedBusCommand(WriteConfigurationGroupA(), i),
            muxSelect);

        auto gpioADCcmd = StartGpioADC(AdcMode::k7k, GpioSelection::k4);
        if (m_bus.SendCommand(LTC681xBus::BuildAddressedBusCommand(
                gpioADCcmd, i)) != LTC681xBus::LTC681xBusStatus::Ok) {
          printf("Things are not okay. StartGPIO ADC\n");
        }

        ThisThread::sleep_for(5ms);

        uint8_t rxbuf[8 * 2];

        m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadAuxiliaryGroupA(), i),
            rxbuf);
        m_bus.SendReadCommand(
            LTC681xBus::BuildAddressedBusCommand(ReadAuxiliaryGroupB(), i),
            rxbuf + 8);

        uint16_t tempVoltage = ((uint16_t)rxbuf[8]) | ((uint16_t)rxbuf[9] << 8);

        int8_t temp = convertTemp(tempVoltage / 10);
        // printf("Temp: %d\n", temp);
        allTemps[j] = temp;
      }

      for (int j = 0; j < 12; j++) {
        // Endianness of the protocol allows a simple cast :-)
        uint16_t voltage = rawVoltages[j] / 10;

        int index = BMS_CELL_MAP[j];
        if (index != -1) {
          allVoltages[(BMS_BANK_CELL_COUNT * i) + index] = voltage;

          // printf("%d: V: %d\n", index, voltage);
        }
      }
    }

    uint16_t minVoltage = allVoltages[0];
    uint16_t maxVoltage = 0;
    for (int i = 0; i < BMS_BANK_COUNT * BMS_BANK_CELL_COUNT; i++) {
      if (allVoltages[i] < minVoltage) {
        minVoltage = allVoltages[i];
      } else if (allVoltages[i] > maxVoltage) {
        maxVoltage = allVoltages[i];
      }
    }

    uint16_t minTemp = allTemps[0];
    uint16_t maxTemp = 0;
    for (int i = 0; i < BMS_BANK_COUNT * BMS_BANK_TEMP_COUNT; i++) {
      if (allTemps[i] < minTemp) {
        minTemp = allTemps[i];
      } else if (allTemps[i] > maxTemp) {
        maxTemp = allTemps[i];
      }
    }

    if (minVoltage <= BMS_FAULT_VOLTAGE_THRESHOLD_LOW ||
        maxVoltage >= BMS_FAULT_VOLTAGE_THRESHOLD_HIGH ||
        minTemp <= BMS_FAULT_TEMP_THRESHOLD_LOW ||
        maxTemp >= BMS_FAULT_TEMP_THRESHOLD_HIGH) {
      throwBmsFault();
    }

    // TODO: DON'T FORGET TO REMOVE THE '!'
    if (!m_discharging) {
        for (int i = 0; i < BMS_BANK_COUNT; i++) {
            
            LTC6811::Configuration config = m_chips[i].getConfig();

            uint16_t dischargeValue = 0x0000; 

            int cellNum = 0;

            for (int j = 0; j < 12; j++) {
                if (BMS_CELL_MAP[j] == -1) {
                    continue;
                }
                uint16_t cellVoltage = allVoltages[i * BMS_BANK_CELL_COUNT + cellNum];
                if (cellVoltage >= BMS_BALANCE_THRESHOLD && cellVoltage >= minVoltage + BMS_DISCHARGE_THRESHOLD) {
                    printf("Balancing cell %d\n", cellNum);
                    dischargeValue &= ~(0x1 << j);
                } else {
                    dischargeValue |= (0x1 << j);
                }
                cellNum++;
            }
            
            //printf("discharge value: %x\n", dischargeValue);

            //m_chips[i].updateConfig();

            //LTC6811::Configuration conf = m_chips[i].getConfig();
            //printf("config: %x\n", conf.dischargeState.value);

            //uint8_t buf[8];

            //uint8_t sendBuf[6] = {0x78, 0x00, 0x00, 0x00, 0xff, 0x0f};

            //ThisThread::sleep_for(5ms);

            //m_bus.SendDataCommand(LTC681xBus::BuildAddressedBusCommand(WriteConfigurationGroupA(), i), sendBuf);

            //ThisThread::sleep_for(15ms);

            //m_bus.SendReadCommand(LTC681xBus::BuildAddressedBusCommand(ReadConfigurationGroupA(), i), buf);

            //ThisThread::sleep_for(5ms);

            //printf("conf group a: %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
            
            //ThisThread::sleep_for(5ms);

        }
    } else {
        for (int i = 0; i < BMS_BANK_COUNT; i++) {

            LTC6811::Configuration& config = m_chips[i].getConfig();
            config.dischargeState.value = 0xff0f;
            m_chips[i].updateConfig();
        }
    }

    for (int i = 0; i < BMS_BANK_COUNT; i++) {
        LTC6811::Configuration& config = m_chips[i].getConfig();
        config.gpio5 = LTC6811::GPIOOutputState::kHigh;
        m_chips[i].updateConfig();
    }

    for (auto mailbox : mailboxes) {
      if (!mailbox->full()) {
        {
          auto msg = new VoltageMeasurement();
          msg->voltageValues = allVoltages;
          mailbox->put((BmsEvent *)msg);
        }

        {
          auto msg = new TemperatureMeasurement();
          msg->temperatureValues = allTemps;
          mailbox->put((BmsEvent *)msg);
        }
      }
    }

    ThisThread::sleep_for(100ms); // TODO: change to 100 or lower
  }
}

void BMSThread::throwBmsFault() {
  m_discharging = false;
  // palClearLine(LINE_BMS_FLT);
  // palSetLine(LINE_CHARGER_CONTROL);
}
