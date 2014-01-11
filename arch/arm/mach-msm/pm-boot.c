/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <asm/cacheflush.h>

#include "scm-boot.h"
#include "idle.h"
#include "pm-boot.h"

static void (*msm_pm_boot_before_pc)(unsigned int cpu, unsigned long entry);
static void (*msm_pm_boot_after_pc)(unsigned int cpu);

static int __devinit msm_pm_tz_boot_init(void)
{
	unsigned int flag = 0;
	if (num_possible_cpus() == 1)
		flag = SCM_FLAG_WARMBOOT_CPU0;
	else if (num_possible_cpus() == 2)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1;
	else if (num_possible_cpus() == 4)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1 |
				SCM_FLAG_WARMBOOT_CPU2 | SCM_FLAG_WARMBOOT_CPU3;
	else
		__WARN();

	return scm_set_boot_addr(virt_to_phys(msm_pm_boot_entry), flag);
}

static void msm_pm_write_boot_vector(unsigned int cpu, unsigned long address)
{
	msm_pm_boot_vector[cpu] = address;

	dmac_clean_range((void *)&msm_pm_boot_vector[cpu],
			(void *)(&msm_pm_boot_vector[cpu] +
				sizeof(msm_pm_boot_vector[cpu])));
}

static void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry)
{
	msm_pm_write_boot_vector(cpu, entry);
}

void msm_pm_boot_config_before_pc(unsigned int cpu, unsigned long entry)
{
	if (msm_pm_boot_before_pc)
		msm_pm_boot_before_pc(cpu, entry);
}

void msm_pm_boot_config_after_pc(unsigned int cpu)
{
	if (msm_pm_boot_after_pc)
		msm_pm_boot_after_pc(cpu);
}

static int __init msm_pm_boot_init(void)
{
	int ret = 0;

	ret = msm_pm_tz_boot_init();
	msm_pm_boot_before_pc = msm_pm_config_tz_before_pc;
	msm_pm_boot_after_pc = NULL;

	return ret;
}

static int __devinit msm_pm_boot_probe(struct platform_device *pdev)
{
	struct msm_pm_boot_platform_data pdata;
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;
	uint32_t vaddr_val;

	pdata.p_addr = 0;
	vaddr_val = 0;

	key = "qcom,mode";
	ret = msm_pm_get_boot_config_mode(pdev->dev.of_node, key, &val);
	if (ret)
		goto fail;
	pdata.mode = val;

	key = "qcom,phy-addr";
	ret = of_property_read_u32(pdev->dev.of_node, key, &val);
	if (!ret)
		pdata.p_addr = val;


	key = "qcom,virt-addr";
	ret = of_property_read_u32(pdev->dev.of_node, key, &vaddr_val);

	switch (pdata.mode) {
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_PHYS:
		if (!pdata.p_addr) {
			key = "qcom,phy-addr";
			goto fail;
		}
		break;
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_VIRT:
		if (!vaddr_val)
			goto fail;

		pdata.v_addr = (void *)vaddr_val;
		break;
	case MSM_PM_BOOT_CONFIG_REMAP_BOOT_ADDR:
		if (!vaddr_val)
			goto fail;

		pdata.v_addr = ioremap_nocache(vaddr_val, SZ_8);

		pdata.p_addr = allocate_contiguous_ebi_nomap(SZ_8, SZ_64K);
		if (!pdata.p_addr) {
			key = "qcom,phy-addr";
			goto fail;
		}
		break;
	case MSM_PM_BOOT_CONFIG_TZ:
		break;
	default:
		pr_err("%s: Unsupported boot mode %d",
			__func__, pdata.mode);
		goto fail;
	}

	return msm_pm_boot_init(&pdata);

fail:
	pr_err("Error reading %s\n", key);
	return -EFAULT;
}

static struct of_device_id msm_pm_match_table[] = {
	{.compatible = "qcom,pm-boot"},
	{},
};

static struct platform_driver msm_pm_boot_driver = {
	.probe = msm_pm_boot_probe,
	.driver = {
		.name = "pm-boot",
		.owner = THIS_MODULE,
		.of_match_table = msm_pm_match_table,
	},
};

static int __init msm_pm_boot_module_init(void)
{
	return platform_driver_register(&msm_pm_boot_driver);
}
postcore_initcall(msm_pm_boot_module_init);
