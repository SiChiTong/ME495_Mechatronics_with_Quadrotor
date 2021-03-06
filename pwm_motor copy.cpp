#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>

#define frequency 25000000.0
#define LED0 0x6			//LED0 start register
#define LED0_ON_L 0x6		//LED0 output and brightness control byte 0
#define LED0_ON_H 0x7		//LED0 output and brightness control byte 1
#define LED0_OFF_L 0x8		//LED0 output and brightness control byte 2
#define LED0_OFF_H 0x9		//LED0 output and brightness control byte 3
#define LED_MULTIPLYER 4	// For the other 15 channels
int execute,pwm,imu;
int value[4]={1000,1000,1000,1000};

////init the imu
void init_imu()
{
  imu=wiringPiI2CSetup(0x6B) ; //accel/gyro address

  int hex_value = wiringPiI2CReadReg8(imu,0x0F); //read who am i register
  printf(" 0x%x \r\n",hex_value);

  //turn on imu by writing 0xC0 to address 0x10 and 0x20
  wiringPiI2CWriteReg8(imu,0x10,0xC0); //turn on and set gyro output
  wiringPiI2CWriteReg8(imu,0x20,0xC0); //turn on and set accell output
}

int get_rpy(float & roll, float &yaw, float &pitch)
{
  //initialize register address for gyroscope
  static int const add_gyro[3]={0x18,0x1A,0x1C};
  //initialize register address for accelerometer
  static int const add_acc[3]={0x28,0x2A,0x2C};
  //initialize calibration value for gyroscope
  static int const gyro_calibration[3]={116,-85,-294};

  //initialize pitch, roll and roll angle
  static float rpy_angle[3]={0};
  // initialize delta_pitch_angle and delta_roll_angle
  static float delta_rpy_angle[3]={0};

  //initialize pitch and roll angle by integrating angular velocity from gyroscope
  static float gyro_angle[2]={0};
  //initialize pitch and roll angle from accelerometer
  static float acc_angle[2]={0};

  static long time_curr;
  static long time_prev;
  struct timespec te;

  int gyro_data[3]={0};
  int acc_data[3]={0};

  for(int i=0;i<3;i++)
  {
    //get angular velocity from gyroscope
    gyro_data[i]=wiringPiI2CReadReg16(imu,add_gyro[i]);
    if(gyro_data[i]>0x8000)
    {
        gyro_data[i]=gyro_data[i] ^ 0xffff;
        gyro_data[i]=-gyro_data[i]-1;
    }
    //calibrate angular velocity
    gyro_data[i]+=gyro_calibration[i];

    //get acceleration from accelerometer
    acc_data[i]=wiringPiI2CReadReg16(imu,add_acc[i]);
    if(acc_data[i]>0x8000)
    {
        acc_data[i]=acc_data[i] ^ 0xffff;
        acc_data[i]=-acc_data[i]-1;
    }
  }

  //get current time in nanoseconds
  timespec_get(&te,TIME_UTC);

  //remember the current time
  time_curr=te.tv_nsec;

  //compute time since last execution
  float diff=time_curr-time_prev;

  //update previous time
  time_prev=time_curr;

  //check for rollover
  if(diff<=0)
  {
    diff+=1000000000;
  }

  //convert to seconds
  diff=diff/1000000000;

  float angle_dps[3]={0};

  for(int i=0;i<3;i++)
  {
    //convert angular velocity to dps
    angle_dps[i]=gyro_data[i]*245.0/32767.0;
    //calculate the change of pitch/roll/yaw angle
    delta_rpy_angle[i]=angle_dps[i]*diff;
    //update pitch/roll/yaw/angle
    rpy_angle[i]+=delta_rpy_angle[i];
  }

  //update roll and pitch by integrating angular velocity from gyroscope
  gyro_angle[0]+=delta_rpy_angle[0]; //update roll
  gyro_angle[1]+=delta_rpy_angle[1]; //update pitch

  //update roll and pitch from accelerometer and convert from rad to deg
  acc_angle[0]=atan2(float(-acc_data[1]),float(acc_data[2]))*180.0/3.1415926;
  acc_angle[1]=atan2(float(acc_data[0]),float(acc_data[2]))*180.0/3.1415926;

  //complementary filtering
  float A=0.02; //complementary filtering coefficient is 0.02
  rpy_angle[0]= A*acc_angle[0]+(1-A)*rpy_angle[0]; //filter roll angle
  rpy_angle[1]= A*acc_angle[1]+(1-A)*rpy_angle[1]; //filter pitch angle

  roll = rpy_angle[0];
  pitch = rpy_angle[1];
  yaw = rpy_angle[2];
}

//init the pwm board
void init_pwm(int pwm)
{
  float freq =400.0*.95;
  float prescaleval = 25000000;
  prescaleval /= 4096;
  prescaleval /= freq;
  prescaleval -= 1;
  uint8_t prescale = floor(prescaleval+0.5);
  int settings = wiringPiI2CReadReg8(pwm, 0x00) & 0x7F;
  int sleep	= settings | 0x10;
  int wake 	= settings & 0xef;
  int restart = wake | 0x80;
  wiringPiI2CWriteReg8(pwm, 0x00, sleep);
  wiringPiI2CWriteReg8(pwm, 0xfe, prescale);
  wiringPiI2CWriteReg8(pwm, 0x00, wake);
  delay(10);
  wiringPiI2CWriteReg8(pwm, 0x00, restart|0x20);
}

//turn on the motor
void init_motor(int pwm,uint8_t channel)
{
	int on_value=0;

	int time_on_us=900;
	uint16_t off_value=round((time_on_us*4096.f)/(1000000.f/400.0));

	wiringPiI2CWriteReg8(pwm, LED0_ON_L + LED_MULTIPLYER * channel, on_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_ON_H + LED_MULTIPLYER * channel, on_value >> 8);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_L + LED_MULTIPLYER * channel, off_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_H + LED_MULTIPLYER * channel, off_value >> 8);
	delay(100);

	 time_on_us=1200;
	 off_value=round((time_on_us*4096.f)/(1000000.f/400.0));

	wiringPiI2CWriteReg8(pwm, LED0_ON_L + LED_MULTIPLYER * channel, on_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_ON_H + LED_MULTIPLYER * channel, on_value >> 8);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_L + LED_MULTIPLYER * channel, off_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_H + LED_MULTIPLYER * channel, off_value >> 8);
	delay(100);

	time_on_us=1000;
	off_value=round((time_on_us*4096.f)/(1000000.f/400.0));

	wiringPiI2CWriteReg8(pwm, LED0_ON_L + LED_MULTIPLYER * channel, on_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_ON_H + LED_MULTIPLYER * channel, on_value >> 8);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_L + LED_MULTIPLYER * channel, off_value & 0xFF);
	wiringPiI2CWriteReg8(pwm, LED0_OFF_H + LED_MULTIPLYER * channel, off_value >> 8);
	delay(100);

}

//set the pwm value of the motor
void set_PWM(int pwm, uint8_t channel, float time_on_us)
{
	//if(pwm<1000)
	//{
		//pwm=1000;
	//}
	uint16_t off_value=round((time_on_us*4096.f)/(1000000.f/400.0));
	wiringPiI2CWriteReg16(pwm, LED0_OFF_L + LED_MULTIPLYER * channel,off_value);

}

//when cntrl+c pressed, kill motors
void trap(int signal)
{
  //turn off  all 4 motors!!

  value[0]=1000;
  value[1]=1000;
  value[2]=1000;
  value[3]=1000;

  for(int i=5;i>0;i--)
  {
    set_PWM(pwm,0,1000);
    set_PWM(pwm,1,1000);
    set_PWM(pwm,2,1000);
    set_PWM(pwm,3,1000);
  }
  execute=0;
  printf("ending program\n\r");
}

int main (int argc, char *argv[])
{
  //open file for writing to if needed
	FILE *f;
	f=fopen("data.txt","a+");
	execute=1;

	signal(SIGINT, &trap);
  struct timespec te;
  int address;
  int mag,imu;
  int data;
	int display;

  wiringPiSetup ();


  //setup for pwm
  pwm=wiringPiI2CSetup (0x40);  //connect pwm board to imu
  init_imu();

  if(imu==-1||pwm==-1)
  {
      printf("cant connect to I2C device %d %d\n",imu,pwm);
      return -1;
  }
  else
  {
    //start the pwm
    init_pwm(pwm);

    //init motor 0
    init_motor(pwm,0);
    init_motor(pwm,1);

    value[0]=1200;
    value[1]=1200;

    int rp_pwm[2]={1250,1250};
    int P=5;
    //start other motor
    float rpy[3]={0,0,0};
    while(execute==1)
    {
      //compute roll/pitch with complementary filter, yaw with integrator
      get_rpy(rpy[0],rpy[1],rpy[2]);
      if(execute==1)//if cntrl+c has not been pressed
      {

        //check imu limit safety
        if(rpy[0]>30 || rpy[0]<-30)
        {
          execute=0;
          value[0]=1000;
          value[1]=1000;
          value[2]=1000;
          value[3]=1000;

          set_PWM(pwm,0,value[0]);
          set_PWM(pwm,1,value[1]);
          set_PWM(pwm,2,value[2]);
          set_PWM(pwm,3,value[3]);
        }
        //if out of limits execute ==0, turn off motors

        //else

        //compute p error and set motors

        //set motors
        //float error = P*rpy[0];
        //value[0] += error;
        //value[1] -= error;

        printf("%f,%d,%d\n",rpy[0],value[0],value[1]);
        set_PWM(pwm,0,value[0]);
        set_PWM(pwm,1,value[1]);
      }
      else
      {
        value[0]=1000;
        value[1]=1000;
        value[2]=1000;
        value[3]=1000;

        set_PWM(pwm,0,value[0]);
        set_PWM(pwm,1,value[1]);
        set_PWM(pwm,2,value[2]);
        set_PWM(pwm,3,value[3]);

        return 0;
        //turn off motors
        //exit program
      }
      //if(display%200==0)
      //{
        //fprintf(f, "comp_roll %f gyro_roll %f accel_rol %f\n\r",roll_angle,-x_gyro_integrated,roll_accel);
        //printf("delta %f roll_angle %f pitch_angle %f yaw_angle %f\n\r",diff,roll_angle,pitch_angle,yaw_angle);
      //}
      //display++;
      ////remember the current time
      //time_prev=time_curr;
    }
   }
        return 0;
}
