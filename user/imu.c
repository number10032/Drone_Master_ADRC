#include "imu.h"
/**************�ļ�˵��**********************
����������У׼,�˲�,�����˲�������Ԫ��
********************************************/

#define T  0.0025    //��������,2.5ms
#define hT 0.00125   //��������/2
#define qT 0.000625  //��������/4
#define Kp 2.0f
#define Ki 0.0f
const float accA[3][3]={
	{0.998710289816700,-0.002469338979608,-0.042756977266890},
	{0.021500654929696,0.994526164619401,-0.053733369334623},
	{0.006340026957332,0.014960289002374,0.985614299769622}};
const float accB[3]={-846,-744,3224};
float gyroB[3]={0,0,0};

/***********************
����IIR��ͨ�˲���ֱ��II�ͽṹ
*@delay:��Ҫ�ݴ�3��״̬�����Ĵ洢�ռ�
*@DataIn:ÿ������������
����˲������������
**********************/
float IIR_LowPassFilter(float DataIn,float *delay)
{
	delay[0] = DataIn + 0.76295*delay[1] - 0.283438*delay[2];
	float DataOut = 0.129*delay[0] + 0.258*delay[1] + 0.129*delay[2];
	delay[2] = delay[1];
	delay[1] = delay[0];
	return DataOut;
}

/***********************
������ȷ����У׼����У�����ٶȼ�ԭʼ����
**********************/
void Acc_Calibrate(AxisInt *acc)
{
	float ax=acc->x,ay=acc->y,az=acc->z;
	acc->x=accA[0][0]*ax+accA[0][1]*ay+accA[0][2]*az+accB[0];
	acc->y=accA[1][0]*ax+accA[1][1]*ay+accA[1][2]*az+accB[1];
	acc->z=accA[2][0]*ax+accA[2][1]*ay+accA[2][2]*az+accB[2];
}

void IMUupdate(AxisInt acc,AxisInt *gyro,Quaternion *Q)
{
	float ax=acc.x,ay=acc.y,az=acc.z;  //��һ�����ٶȼ������ݴ�
	if(ax==0 && ay==0 && az==0)return;
	float q0=Q->q0,q1=Q->q1,q2=Q->q2,q3=Q->q3;  //��Ԫ���ݴ�
	if(q0==0 && q1==0 && q2==0 && q3==0)return;
	static float ogx=0,ogy=0,ogz=0;  //��һʱ�̵Ľ��ٶ�
	static float oq0=1,oq1=0,oq2=0,oq3=0;  //��һʱ�̵���Ԫ��
	static float exInt=0,eyInt=0,ezInt=0;
	//�������ٶȹ�һ��
	float norm=Q_rsqrt(ax*ax+ay*ay+az*az);
	ax*=norm;ay*=norm;az*=norm;
	//��ȡ��Ԫ���ĵ�Ч���Ҿ����е���������
	float vx=2*(q1*q3-q0*q2);
	float vy=2*(q0*q1+q2*q3);
	float vz=1-2*(q1*q1+q2*q2);
	//��������ó���̬���
	float ex=ay*vz-az*vy; 
	float ey=az*vx-ax*vz;
	float ez=ax*vy-ay*vx;
	//�������л���
	exInt+=ex*Ki;
	eyInt+=ey*Ki;
	ezInt+=ez*Ki;
	//��̬���������ٶ���,�������ٶȻ���Ư��
	float gx=GyroToRad(gyro->x)+Kp*ex+exInt;
	float gy=GyroToRad(gyro->y)+Kp*ey+eyInt;
	float gz=GyroToRad(gyro->z)+Kp*ez+ezInt;
	//�Ľ�ŷ������ֵ�����Ԫ��΢�ַ���
	float K0=-oq1*ogx-oq2*ogy-oq3*ogz;
	float K1=oq0*ogx-oq3*ogy+oq2*ogz;
	float K2=oq3*ogx+oq0*ogy-oq1*ogz;
	float K3=-oq2*ogx+oq1*ogy+oq0*ogz;
	K0+=-q1*gx-q2*gy-q3*gz;
	K1+=q0*gx-q3*gy+q2*gz;
	K2+=q3*gx+q0*gy-q1*gz;
	K3+=-q2*gx+q1*gy+q0*gz;
	q0+=qT*K0;
	q1+=qT*K1;
	q2+=qT*K2;
	q3+=qT*K3;
	//��Ԫ����һ�������,��ֵ����
	norm=Q_rsqrt(q0*q0+q1*q1+q2*q2+q3*q3);
	Q->q0=q0*norm;Q->q1=q1*norm;Q->q2=q2*norm;Q->q3=q3*norm;
	oq0=Q->q0;oq1=Q->q1;oq2=Q->q2;oq3=Q->q3;
	ogx=gx;ogy=gy;ogz=gz;
	gyro->x=RadToGyro(gx);
	gyro->y=RadToGyro(gy);
	gyro->z=RadToGyro(gz);
}