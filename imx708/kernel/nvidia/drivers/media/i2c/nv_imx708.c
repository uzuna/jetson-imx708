/*
 * imx708.c - imx708 sensor driver
 *
 * Copyright (c) 2020, RidgeRun. All rights reserved.
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (c) 2022, Raspberry Pi Ltd
 * Copyright (c) 2023, FUJINAKA Fumiya<uzuna.kf@gmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define DEBUG 1

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegra_v4l2_camera.h>
#include <media/tegracam_core.h>

#include "../platform/tegra/camera/camera_gpio.h"
#include "imx708_mode_tbls.h"

#define IMX708_SENSOR_INTERNAL_CLK_FREQ   840000000
#define IMX708_REG_CHIP_ID		0x0016
#define IMX708_CHIP_ID			0x0708

static const struct of_device_id imx708_of_match[] = {
	{.compatible = "sony,imx708",},
	{},
};

MODULE_DEVICE_TABLE(of, imx708_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
};

enum imx708_Config {
	TWO_LANE_CONFIG,
	FOUR_LANE_CONFIG,
};

struct imx708 {
	struct i2c_client *i2c_client;
	struct v4l2_subdev *subdev;
	u16 fine_integ_time;
	u32 frame_length;
	struct camera_common_data *s_data;
	struct tegracam_device *tc_dev;
	enum imx708_Config config;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
		.use_single_rw = true,
#else
		.use_single_read = true,
		.use_single_write = true,
#endif
};

static inline int imx708_read_reg(struct camera_common_data *s_data,
				  u16 addr, u8 * val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xff;

	return err;
}

static inline int imx708_write_reg(struct camera_common_data *s_data,
				   u16 addr, u8 val)
{
	int err = 0;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(s_data->dev, "%s: i2c write failed, 0x%x = %x",
			__func__, addr, val);

	return err;
}

static int imx708_write_table(struct imx708 *priv, const imx708_reg table[])
{
	return regmap_util_write_table_8(priv->s_data->regmap, table, NULL, 0,
					 IMX708_TABLE_WAIT_MS,
					 IMX708_TABLE_END);
}

// 値が8bit以上で連続するレジスタに書き込む時に使う
static void imx708_regtable(imx708_reg table[], u16 address_low, u8 nr_regs,
	u32 value)
{
	unsigned int i;

	for (i = 0; i < nr_regs; i++) {
		(table+i)->addr = address_low + i;
		(table+i)->val = (u8)(value >> ((nr_regs - 1 - i) * 8));
	}
}

static int imx708_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	return 0;
}

static int imx708_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx708 *priv = (struct imx708 *)tc_dev->priv;
	struct device *dev = s_data->dev;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];

	imx708_reg regs[3];

	if (val < mode->control_properties.min_gain_val)
		val = mode->control_properties.min_gain_val;
	else if (val > mode->control_properties.max_gain_val)
		val = mode->control_properties.max_gain_val;

	dev_dbg(dev, "%s: val: %lld\n", __func__, val);
	// アナログゲインは2レジスタに分かれている
	// 単位が不明なので、値の変換をせずそのまま書き込む
	imx708_regtable(regs, IMX708_REG_ANALOG_GAIN, 2, val);
	dev_dbg(dev, "%s: val: %lld\n", __func__, val);
	(regs+2)->addr = IMX708_TABLE_END;

	return imx708_write_table(priv, regs);
}

static int imx708_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	return 0;
}

static int imx708_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx708 *priv = (struct imx708 *)tc_dev->priv;
	struct device *dev = s_data->dev;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	
	u32 exp = 0;
	imx708_reg regs[3];

	if (val < (s64)mode->control_properties.min_exp_time.val)
		val = (s64)mode->control_properties.min_exp_time.val;
	else if (val > (s64)mode->control_properties.max_exp_time.val)
		val = (s64)mode->control_properties.max_exp_time.val;
	
	// 露光時間はline数となっているが、デバイスツリーでは時間で表記している
	// 時間をline数に変換する
	exp = (u32)(val / (s64)mode->control_properties.step_exp_time.val);
	dev_dbg(dev, "%s: exp_time: %lld [%u]\n", __func__, val, exp);
	imx708_regtable(regs, IMX708_REG_EXPOSURE, 2, exp);
	(regs+2)->addr = IMX708_TABLE_END;
	return imx708_write_table(priv, regs);
}

static struct tegracam_ctrl_ops imx708_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = imx708_set_gain,
	.set_exposure = imx708_set_exposure,
	.set_frame_rate = imx708_set_frame_rate,
	.set_group_hold = imx708_set_group_hold,
};

static int imx708_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio) {
		if (gpio_cansleep(pw->reset_gpio))
			gpio_set_value_cansleep(pw->reset_gpio, 0);
		else
			gpio_set_value(pw->reset_gpio, 0);
	}

	if (unlikely(!(pw->avdd || pw->iovdd || pw->dvdd)))
		goto skip_power_seqn;

	usleep_range(10, 20);

	if (pw->avdd) {
		err = regulator_enable(pw->avdd);
		if (err)
			goto imx708_avdd_fail;
	}

	if (pw->iovdd) {
		err = regulator_enable(pw->iovdd);
		if (err)
			goto imx708_iovdd_fail;
	}

	if (pw->dvdd) {
		err = regulator_enable(pw->dvdd);
		if (err)
			goto imx708_dvdd_fail;
	}

	usleep_range(10, 20);

skip_power_seqn:
	if (pw->reset_gpio) {
		if (gpio_cansleep(pw->reset_gpio))
			gpio_set_value_cansleep(pw->reset_gpio, 1);
		else
			gpio_set_value(pw->reset_gpio, 1);
	}

	usleep_range(8000, 9000);

	pw->state = SWITCH_ON;

	return 0;

imx708_dvdd_fail:
	regulator_disable(pw->iovdd);

imx708_iovdd_fail:
	regulator_disable(pw->avdd);

imx708_avdd_fail:
	dev_err(dev, "%s failed.\n", __func__);

	return -ENODEV;
}

static int imx708_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power off\n", __func__);

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (err) {
			dev_err(dev, "%s failed.\n", __func__);
			return err;
		}
	} else {
		if (pw->reset_gpio) {
			if (gpio_cansleep(pw->reset_gpio))
				gpio_set_value_cansleep(pw->reset_gpio, 0);
			else
				gpio_set_value(pw->reset_gpio, 0);
		}

		usleep_range(10, 10);

		if (pw->dvdd)
			regulator_disable(pw->dvdd);
		if (pw->iovdd)
			regulator_disable(pw->iovdd);
		if (pw->avdd)
			regulator_disable(pw->avdd);
	}

	pw->state = SWITCH_OFF;

	return 0;
}

static int imx708_power_put(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->dvdd))
		devm_regulator_put(pw->dvdd);

	if (likely(pw->avdd))
		devm_regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		devm_regulator_put(pw->iovdd);

	pw->dvdd = NULL;
	pw->avdd = NULL;
	pw->iovdd = NULL;

	if (likely(pw->reset_gpio)){
		dev_dbg(dev, "free reset_gpio\n");
		gpio_free(pw->reset_gpio);
	}

	return 0;
}

static int imx708_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct clk *parent;
	int err = 0;

	if (!pdata) {
		dev_err(dev, "pdata missing\n");
		return -EFAULT;
	}

	/* Sensor MCLK (aka. INCK) */
	if (pdata->mclk_name) {
		pw->mclk = devm_clk_get(dev, pdata->mclk_name);
		if (IS_ERR(pw->mclk)) {
			dev_err(dev, "unable to get clock %s\n",
				pdata->mclk_name);
			return PTR_ERR(pw->mclk);
		}

		if (pdata->parentclk_name) {
			parent = devm_clk_get(dev, pdata->parentclk_name);
			if (IS_ERR(parent)) {
				dev_err(dev, "unable to get parent clock %s",
					pdata->parentclk_name);
			} else
				clk_set_parent(pw->mclk, parent);
		}
	}

	/* analog 2.8v */
	if (pdata->regulators.avdd)
		err |= camera_common_regulator_get(dev,
						   &pw->avdd,
						   pdata->regulators.avdd);
	/* IO 1.8v */
	if (pdata->regulators.iovdd)
		err |= camera_common_regulator_get(dev,
						   &pw->iovdd,
						   pdata->regulators.iovdd);
	/* dig 1.2v */
	if (pdata->regulators.dvdd)
		err |= camera_common_regulator_get(dev,
						   &pw->dvdd,
						   pdata->regulators.dvdd);
	if (err) {
		dev_err(dev, "%s: unable to get regulator(s)\n", __func__);
		goto done;
	}

	/* Reset or ENABLE GPIO */
	pw->reset_gpio = pdata->reset_gpio;
	err = gpio_request(pw->reset_gpio, "cam_reset_gpio");
	if (err < 0) {
		dev_err(dev, "%s: unable to request reset_gpio (%d)\n",
			__func__, err);
		goto done;
	}

done:
	pw->state = SWITCH_OFF;

	return err;
}

static struct camera_common_pdata *imx708_parse_dt(struct tegracam_device
						   *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *np = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	struct camera_common_pdata *ret = NULL;
	int err = 0;
	int gpio;

	if (!np)
		return NULL;

	match = of_match_device(imx708_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev,
					sizeof(*board_priv_pdata), GFP_KERNEL);
	if (!board_priv_pdata)
		return NULL;

	gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (gpio < 0) {
		if (gpio == -EPROBE_DEFER)
			ret = ERR_PTR(-EPROBE_DEFER);
		dev_err(dev, "reset-gpios not found\n");
		goto error;
	}
	board_priv_pdata->reset_gpio = (unsigned int)gpio;

	err = of_property_read_string(np, "mclk", &board_priv_pdata->mclk_name);
	if (err)
		dev_dbg(dev, "mclk name not present, "
			"assume sensor driven externally\n");

	err = of_property_read_string(np, "avdd-reg",
				      &board_priv_pdata->regulators.avdd);
	err |= of_property_read_string(np, "iovdd-reg",
				       &board_priv_pdata->regulators.iovdd);
	err |= of_property_read_string(np, "dvdd-reg",
				       &board_priv_pdata->regulators.dvdd);
	if (err)
		dev_dbg(dev, "avdd, iovdd and/or dvdd reglrs. not present, "
			"assume sensor powered independently\n");

	board_priv_pdata->has_eeprom = of_property_read_bool(np, "has-eeprom");

	return board_priv_pdata;

error:
	devm_kfree(dev, board_priv_pdata);

	return ret;
}

static int imx708_set_mode(struct tegracam_device *tc_dev)
{
	struct imx708 *priv = (struct imx708 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;

	int err = 0;
	const char *config;
	struct device_node *mode;
	uint offset = ARRAY_SIZE(imx708_frmfmt);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	mode = of_get_child_by_name(tc_dev->dev->of_node, "mode0");
	err = of_property_read_string(mode, "num_lanes", &config);

	if (config[0] == '4')
		priv->config = FOUR_LANE_CONFIG;
	else if (config[0] == '2')
		priv->config = TWO_LANE_CONFIG;
	else
		dev_err(tc_dev->dev, "Unsupported config\n");

	err = imx708_write_table(priv, mode_table[IMX708_MODE_COMMON]);
	if (err)
		return err;

	if (priv->config == FOUR_LANE_CONFIG)
		err = imx708_write_table(priv, mode_table[s_data->mode + offset]);
	else
		err = imx708_write_table(priv, mode_table[s_data->mode]);
	if (err)
		return err;

	return 0;
}

static int imx708_start_streaming(struct tegracam_device *tc_dev)
{
	struct imx708 *priv = (struct imx708 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	return imx708_write_table(priv, mode_table[IMX708_START_STREAM]);
}

static int imx708_stop_streaming(struct tegracam_device *tc_dev)
{
	int err;
	struct imx708 *priv = (struct imx708 *)tegracam_get_privdata(tc_dev);

	dev_dbg(tc_dev->dev, "%s:\n", __func__);
	err = imx708_write_table(priv, mode_table[IMX708_STOP_STREAM]);

	return err;
}

static struct camera_common_sensor_ops imx708_common_ops = {
	.numfrmfmts = ARRAY_SIZE(imx708_frmfmt),
	.frmfmt_table = imx708_frmfmt,
	.power_on = imx708_power_on,
	.power_off = imx708_power_off,
	.write_reg = imx708_write_reg,
	.read_reg = imx708_read_reg,
	.parse_dt = imx708_parse_dt,
	.power_get = imx708_power_get,
	.power_put = imx708_power_put,
	.set_mode = imx708_set_mode,
	.start_streaming = imx708_start_streaming,
	.stop_streaming = imx708_stop_streaming,
};

static int imx708_board_setup(struct imx708 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	u8 reg_val[2];
	int err = 0;

	// Skip mclk enable as this camera has an internal oscillator

	err = imx708_power_on(s_data);
	if (err) {
		dev_err(dev, "error during power on sensor (%d)\n", err);
		goto done;
	}

	/* Probe sensor model id registers */
	err = imx708_read_reg(s_data, IMX708_REG_CHIP_ID, &reg_val[0]);
	if (err) {
		dev_err(dev, "%s: error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}
	err = imx708_read_reg(s_data, IMX708_REG_CHIP_ID+1, &reg_val[1]);
	if (err) {
		dev_err(dev, "%s: error during i2c read probe (%d)\n",
			__func__, err);
		goto err_reg_probe;
	}

	if (!((reg_val[0] == 0x07) && reg_val[1] == 0x08))
		dev_err(dev, "%s: invalid sensor model id: %x%x\n",
			__func__, reg_val[0], reg_val[1]);

err_reg_probe:
	imx708_power_off(s_data);

done:
	return err;
}

static int imx708_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops imx708_subdev_internal_ops = {
	.open = imx708_open,
};

static int imx708_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct tegracam_device *tc_dev;
	struct imx708 *priv;
	int err;

	dev_dbg(dev, "probing v4l2 sensor at addr 0x%0x\n", client->addr);

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct imx708), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	tc_dev = devm_kzalloc(dev, sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, "imx708", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &imx708_common_ops;
	tc_dev->v4l2sd_internal_ops = &imx708_subdev_internal_ops;
	tc_dev->tcctrl_ops = &imx708_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	err = imx708_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_dbg(dev, "detected imx708 sensor\n");

	return 0;
}

static int imx708_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct imx708 *priv = (struct imx708 *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	return 0;
}

static const struct i2c_device_id imx708_id[] = {
	{"imx708", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, imx708_id);

static struct i2c_driver imx708_i2c_driver = {
	.driver = {
		   .name = "imx708",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(imx708_of_match),
		   },
	.probe = imx708_probe,
	.remove = imx708_remove,
	.id_table = imx708_id,
};

module_i2c_driver(imx708_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Sony IMX708");
MODULE_AUTHOR("FUJINAKA Fumiya<uzuna.kf@gmail.com>");
MODULE_LICENSE("GPL v2");
