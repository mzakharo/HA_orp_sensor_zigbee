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
            reporting: {
                min: 1,        
                max: 10,       
                change: 1    
            },
        }),
        m.numeric({
            name: "orp_calibration",
            cluster: "genBasic",
            attribute: 0xF000,
            description: "ORP calibration offset",
            unit: "mV",
            precision: 1,
            access: "ALL",
            valueMin: -500,
            valueMax: 500,
            reporting: null,
        })
    ],
    meta: {},
};
