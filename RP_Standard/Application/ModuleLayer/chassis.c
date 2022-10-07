#include "chassis.h"

// 子结构体（便于编写与查看）
drv_can_t				  *chas_drv[CHAS_MOTOR_CNT];
motor_3508_t			*chas_motor[CHAS_MOTOR_CNT];
motor_3508_info_t	*chas_motor_info[CHAS_MOTOR_CNT];
int16_t            out[CHAS_MOTOR_CNT];

float target;

void Chassis_Init(void);
void Chassis_Ctrl(void);
void Chassis_Test(void);
void Chassis_SelfProtect(void);

// 底盘模块传感器
chassis_dev_t		chas_dev = {
	.chas_motor[CHAS_LF] = &chassis_motor[CHAS_LF],
	.chas_motor[CHAS_RF] = &chassis_motor[CHAS_RF],
	.chas_motor[CHAS_LB] = &chassis_motor[CHAS_LB],
	.chas_motor[CHAS_RB] = &chassis_motor[CHAS_RB],
	.imu_sensor = &imu_sensor,
	.rc_sensor = &rc_sensor,
};

// 底盘模块信息
chassis_info_t 	chas_info = {
	.remote_mode = RC,
	.local_mode = CHASSIS_MODE_NORMAL,
};
chassis_conf_t chas_conf = {
	.limit_speed = 8000.0f,
	.machine_round_kp = 3.0f,
};
chassis_t chassis = {
	.dev = &chas_dev,
	.info = &chas_info,
	.conf = &chas_conf,
	.init = Chassis_Init,
	.test = Chassis_Test,
	.ctrl = Chassis_Ctrl,
	.self_protect = Chassis_SelfProtect,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 底盘电机初始化
void Chassis_Motor_Init(void)
{
	chassis_motor[CHAS_LF].init(&chassis_motor[CHAS_LF]);
	chassis_motor[CHAS_RF].init(&chassis_motor[CHAS_RF]);
	chassis_motor[CHAS_LB].init(&chassis_motor[CHAS_LB]);
	chassis_motor[CHAS_RB].init(&chassis_motor[CHAS_RB]);
}

void Chassis_Init(void)
{
	// 底盘子结构体初始化
	chas_motor[CHAS_LF] = chas_dev.chas_motor[CHAS_LF];
	chas_motor[CHAS_RF] = chas_dev.chas_motor[CHAS_RF];
	chas_motor[CHAS_LB] = chas_dev.chas_motor[CHAS_LB];
	chas_motor[CHAS_RB] = chas_dev.chas_motor[CHAS_RB];	
	
	chas_motor_info[CHAS_LF] = chas_dev.chas_motor[CHAS_LF]->info;
	chas_motor_info[CHAS_RF] = chas_dev.chas_motor[CHAS_RF]->info;
	chas_motor_info[CHAS_LB] = chas_dev.chas_motor[CHAS_LB]->info;
	chas_motor_info[CHAS_RB] = chas_dev.chas_motor[CHAS_RB]->info;
	
	chas_drv[CHAS_LF] = chas_dev.chas_motor[CHAS_LF]->driver;
	chas_drv[CHAS_RF] = chas_dev.chas_motor[CHAS_RF]->driver;
	chas_drv[CHAS_LB] = chas_dev.chas_motor[CHAS_LB]->driver;
	chas_drv[CHAS_RB] = chas_dev.chas_motor[CHAS_RB]->driver;
	
	// 底盘电机初始化
	Chassis_Motor_Init();

}

void Chassis_GetRcInfo(void)
{
	int16_t target_cycle_speed = 0;
	if (gimbal.info->yaw_mode == G_Y_follow)
	{
		chassis.info->target_front_speed = (float)rc_sensor.info->ch3 * chas_conf.limit_speed / 660.0f;
		chassis.info->target_right_speed = (float)rc_sensor.info->ch2 * chas_conf.limit_speed / 660.0f;
		chassis.info->target_cycle_speed = (float)rc_sensor.info->ch0 * chas_conf.limit_speed / 660.0f;
	}
	else if (gimbal.info->yaw_mode == G_Y_gyro)
	{
		chassis.info->target_front_speed = (float)rc_sensor.info->ch3 * chas_conf.limit_speed / 660.0f;
		chassis.info->target_right_speed = (float)rc_sensor.info->ch2 * chas_conf.limit_speed / 660.0f;
		target_cycle_speed = gimbal.conf->restart_yaw_motor_angle - yaw_motor.info->ecd;
		
		if (target_cycle_speed > HALF_ECD_RANGE)
		{
			target_cycle_speed -= ECD_RANGE;
		}
		else if (target_cycle_speed < -HALF_ECD_RANGE)
		{
			target_cycle_speed += ECD_RANGE;
		}
		
		chassis.info->target_cycle_speed = (float)target_cycle_speed * chas_conf.machine_round_kp;
	}
}

void Chassis_GetKeyInfo(void)
{
	
}

void Chassis_GetInfo(void)
{
	if (flag.gimbal_flag.reset_ok == 1)
	{
//		if (rc_sensor.info->ch0 == 0 \
//			&& rc_sensor.info->ch1 == 0 \
//			&& rc_sensor.info->ch2 == 0 \
//			&& rc_sensor.info->ch3 == 0 \
//		  && flag.gimbal_flag.reset_start == 0)
//		{
//			flag.gimbal_flag.reset_ok = 1;
//		}
//	}
//	else
//	{
		if(chas_info.remote_mode == RC) {
			Chassis_GetRcInfo();
		}
		else if(chas_info.remote_mode == KEY) {
			Chassis_GetKeyInfo();
		}
	}
}

// 底盘电机卸力
static void Chassis_Stop(chassis_dev_t *chas_dev)
{
	  static uint32_t time = 0, now = 0, stop_init = 0;
		if (flag.chassis_flag.stop_start == 1 && flag.chassis_flag.stop_ok == 0)
		{
			  if (stop_init == 0)
				{
					time = micros();
					stop_init = 1;
				}
				
				now = micros();
				if (now - time > 2000000)
				{
					flag.chassis_flag.stop_start = 0;
					flag.chassis_flag.stop_ok = 1;
					stop_init = 0;
					now = 0;
					time = 0;
				}
				
				for(uint8_t i = 0; i < CHAS_MOTOR_CNT; i++)
				{
					  chas_motor[i]->pid->speed.set = 0.0f;
						chas_motor[i]->pid->speed_out = PID_Plc_Calc(&chas_motor[i]->pid->speed, 
																												chas_motor[i]->info->speed_rpm, //measure
																												chas_motor[i]->pid->speed.set); //target
						out[i] = (int16_t)chas_motor[i]->pid->speed_out;
				}
				can1_tx_buf[(chas_drv[CHAS_LF]->rx_id - 0x201) * 2] = (out[CHAS_LF] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_LF]->rx_id - 0x201) * 2 + 1] = (out[CHAS_LF] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_RF]->rx_id - 0x201) * 2] = (out[CHAS_RF] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_RF]->rx_id - 0x201) * 2 + 1] = (out[CHAS_RF] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_LB]->rx_id - 0x201) * 2] = (out[CHAS_LB] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_LB]->rx_id - 0x201) * 2 + 1] = (out[CHAS_LB] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_RB]->rx_id - 0x201) * 2] = (out[CHAS_RB] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_RB]->rx_id - 0x201) * 2 + 1] = (out[CHAS_RB] & 0xFF);
		}
		else
		{
				for(uint8_t i = 0; i < CHAS_MOTOR_CNT; i++)
				{
						chas_motor[i]->pid->speed_out = 0.0f;
						out[i] = (int16_t)chas_motor[i]->pid->speed_out;
				}
				can1_tx_buf[(chas_drv[CHAS_LF]->rx_id - 0x201) * 2] = (out[CHAS_LF] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_LF]->rx_id - 0x201) * 2 + 1] = (out[CHAS_LF] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_RF]->rx_id - 0x201) * 2] = (out[CHAS_RF] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_RF]->rx_id - 0x201) * 2 + 1] = (out[CHAS_RF] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_LB]->rx_id - 0x201) * 2] = (out[CHAS_LB] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_LB]->rx_id - 0x201) * 2 + 1] = (out[CHAS_LB] & 0xFF);
				can1_tx_buf[(chas_drv[CHAS_RB]->rx_id - 0x201) * 2] = (out[CHAS_RB] >> 8) & 0xFF;
				can1_tx_buf[(chas_drv[CHAS_RB]->rx_id - 0x201) * 2 + 1] = (out[CHAS_RB] & 0xFF);
		}
}

void Chassis_SelfProtect(void)
{
	Chassis_Stop(&chas_dev);
//	Chassis_PidParamsInit(chas_motor_pid, CHAS_MOTOR_CNT);
	Chassis_GetInfo();
}

// 底盘电机速度环计算
static void Chassis_Speed_PidCalc(chassis_dev_t *chas_dev, chassis_motor_cnt_t MOTORx)
{
		chas_motor[MOTORx]->pid->speed_out = PID_Plc_Calc(&chas_motor[MOTORx]->pid->speed, 
	                                                chas_motor[MOTORx]->info->speed_rpm, //measure
                                                  chas_motor[MOTORx]->pid->speed.set); //target
		out[MOTORx] = (int16_t)chas_motor[MOTORx]->pid->speed_out;
}

// 添加底盘电机输出至发送缓存
static void Chassis_SendPidOut(chassis_dev_t *chas_dev)
{
	for(uint8_t i = 0; i < CHAS_MOTOR_CNT; i++) 
	{
		if(chas_motor[i]->work_state == DEV_ONLINE) 
		{
			can1_tx_buf[(chas_motor[i]->driver->rx_id - 0x201) * 2] = (out[i] >> 8) & 0xFF;
			can1_tx_buf[(chas_motor[i]->driver->rx_id - 0x201) * 2 + 1] = (out[i] & 0xFF);
		} 
		else 
		{
			can1_tx_buf[(chas_motor[i]->driver->rx_id - 0x201) * 2] = 0;
			can1_tx_buf[(chas_motor[i]->driver->rx_id - 0x201) * 2 + 1] = 0;
		}
	}    
}

void Chassis_PidCtrl(void)
{
	// 底盘电机速度环计算
	Chassis_Speed_PidCalc(&chas_dev, CHAS_LF);
	Chassis_Speed_PidCalc(&chas_dev, CHAS_RF);
	Chassis_Speed_PidCalc(&chas_dev, CHAS_LB);
	Chassis_Speed_PidCalc(&chas_dev, CHAS_RB);
	
	// 添加底盘电机输出至发送缓存
	Chassis_SendPidOut(&chas_dev);
}

void Chassis_RcCtrl(void)
{
	float front, right, round, sum;
	
	front = chassis.info->target_front_speed;
	right = chassis.info->target_right_speed;
	round = chassis.info->target_cycle_speed;
	
	chas_motor[CHAS_LF]->pid->speed.set = front + right + round;
  chas_motor[CHAS_RF]->pid->speed.set = - front + right + round;
  chas_motor[CHAS_LB]->pid->speed.set = front - right + round;
  chas_motor[CHAS_RB]->pid->speed.set = - front - right + round;
	
	arm_abs_f32(&front, &front, 1);
	arm_abs_f32(&right, &right, 1);
	arm_abs_f32(&round, &round, 1);
	
	// sum = front + right + round;
	arm_add_f32(&front, &right, &sum, 1);
	arm_add_f32(&sum, &round, &sum, 1);
	if (sum > 8000.0f)
	{
		for (uint8_t i = 0; i < CHAS_MOTOR_CNT; i++)
		{
			// chas_motor[i]->pid->speed.set = chas_motor[i]->pid->speed.set / sum * chas_info.limit_speed;
			chas_motor[i]->pid->speed.set /= sum;
      arm_mult_f32(&chas_motor[i]->pid->speed.set, &chas_conf.limit_speed, &chas_motor[i]->pid->speed.set, 1);			
		}
	}
}

void Chassis_Ctrl(void)
{
	/*----信息读入----*/
	Chassis_GetInfo();
	/*----控制方式修改----*/ 
	if(chas_info.remote_mode == RC) {
		Chassis_RcCtrl();
	}
//	else if(chas_info.remote_mode == KEY) {
//		Chassis_KeyCtrl();
//	}
	/*----最终输出----*/
	Chassis_PidCtrl();
}

void Chassis_Test(void)
{
    
}

