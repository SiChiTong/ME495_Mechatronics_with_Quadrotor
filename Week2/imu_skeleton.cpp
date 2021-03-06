#include <stdio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <time.h>
#include <math.h>

int main (int argc, char *argv[])
{
    //open file for writing to if needed
    FILE *f;
    f=fopen("data.txt","a+");
    
    struct timespec te;
    int address;
    int mag,imu;
    int data;
    int hex_value;
    
    wiringPiSetup();
    
    //setup imu on I2C
    imu=wiringPiI2CSetup(0x6B) ; //accel/gyro address
    hex_value = wiringPiI2CReadReg8(imu,0x0F); //read who am i register
    printf(" 0x%x \r\n",hex_value);
    
    if(imu==-1)
    {
        printf("cant connect to I2C device\n");
        return -1;
    }
    else
    {
        //turn on imu by writing 0xC0 to address 0x10 and 0x20
        wiringPiI2CWriteReg8(imu,0x20,0xC0); //turn on and set accell output
        wiringPiI2CWriteReg8(imu,0x10,0xC0); //turn on and set gyro output
        float pitch_angle=0;
        float roll_angle=0;
        float yaw_angle=0;
        long time_curr;
        long time_prev;
        float delta_x_rotate=0;
        float delta_y_rotate=0;
        float delta_z_rotate=0;
        
        while(1)
        {
            //read in gyro z rate
            address=0x1C;
            int z_rate;
            z_rate=wiringPiI2CReadReg16(imu,address);
            
            // convert to 2's complement
            if(z_rate>0x8000){
                z_rate=z_rate ^ 0xffff;
                z_rate=-z_rate-1;
            }
            
            //calibration value
            z_rate-=330;
            
            //convert to dps
            float z_angle_dps=z_rate*245.0/32767.0;
            
            //get current time in nanoseconds
            timespec_get(&te,TIME_UTC);
            time_curr=te.tv_nsec;
            
            //compute time since last execution
            float diff=time_curr-time_prev;
            
            //check for rollover
            if(diff<=0){
                diff+=1000000000;
            }
            
            //convert to seconds
            diff=diff/1000000000;
            
            //compute amount of rotation since last execution
            delta_z_rotate=z_angle_dps*diff;
            
            //compute yaw
            yaw_angle+= delta_z_rotate;
            
            //print out the value
            printf("delta z = %f , yaw angle %f \r\n",delta_z_rotate, yaw_angle);
            
            // //print to file if desired
            //fprintf(f, "yaw %f\n\r",yaw_angle);
            
            //remember the current time
            time_prev=time_curr;
        }
        
  	 }
    return 0;
}
