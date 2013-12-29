#ifndef _ARM_PSCI_H_
#define _ARM_PSCI_H_

extern int psci_init(void);

struct psci_power_state;

struct secondary_data {
	long	stack;
	long	entry_point;
};

extern struct secondary_data secondary_data;

extern int psci_cpu_on(unsigned long cpuid, unsigned long entry_point);
extern int psci_cpu_off(struct psci_power_state state);

#endif /* _ARM_PSCI_H_ */
