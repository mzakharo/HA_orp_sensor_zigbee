import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

export default {
    zigbeeModel: ['esp32c6'],
    model: 'esp32c6',
    vendor: 'ESPRESSIF',
    description: 'ESP32-C6 ORP Sensor',
    extend: [
        m.numeric({
            name: "orp",
            cluster: "genAnalogInput",
            attribute: "presentValue",
            description: "ORP measurement",
            unit: "mV",
            precision: 1,
            access: "STATE_GET",
            reporting: null,
        })
    ],
    meta: {},
};