#ifndef LOW_VOLTAGE_H
#define LOW_VOLTAGE_H


void init_low_voltage(void);

int get_battery_voltage(void);

void low_power(bool STATE);


#endif /* LOW_VOLTAGE_H */
