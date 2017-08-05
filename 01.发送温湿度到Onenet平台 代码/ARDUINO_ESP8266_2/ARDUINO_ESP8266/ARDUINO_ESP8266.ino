#include <TimerOne.h>
#include <HttpPacket.h>
#include <ArduinoJson.h>
#include <dht11.h>
dht11 DHT11;
#define DHT11PIN 8 //DHT11 连接ARDUINO 8
#define RELAY 4 	//继电器控制引脚


HttpPacketHead packet;


#define DebugSerial Serial
#define ESP8266Serail Serial3

#define Success 1U
#define Failure 0U

int L = 13;	//LED指示灯引脚

unsigned long  Time_Cont = 0;		//定时器计数器


const unsigned int esp8266RxBufferLength = 600;
char esp8266RxBuffer[esp8266RxBufferLength];


unsigned int ii = 0;
char OneNetServer[] = "api.heclouds.com";		//不需要修改

const char ssid[] = "CU_4qh9";		//修改为自己的路由器用户名
const char password[] = "aqabyf8k";	//修改为自己的路由器密码



char device_id[] = "9711378";	//修改为自己的设备ID
char API_KEY[] = "NLUZAFLJRNh1IhSHthWh79dm2Os=";	//修改为自己的API_KEY
char sensor_id1[] = "TEMP";
char sensor_id2[] = "HUMI";


void clrEsp8266RxBuffer(void);
void esp8266ReadBuffer();
void Timer1_handler(void);
unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry);
void ESP8266_ERROR(int num);
void initEsp8266();
void postDataToOneNet(char* API_VALUE_temp,char* device_id_temp,char* sensor_id_temp,double thisData);




void setup() {
	pinMode(L, OUTPUT);
	digitalWrite(L, LOW);
	DebugSerial.begin(9600);
	ESP8266Serail.begin(115200);

	Timer1.initialize(1000);
	Timer1.attachInterrupt(Timer1_handler);

	initEsp8266();


	DebugSerial.println("setup end!");
}

void loop() {
	//获取温湿度数据
	int chk = DHT11.read(DHT11PIN);								//读取温湿度值

	  //串口调试DHT11输出信息
	  DebugSerial.print("Read sensor: ");
	  switch (chk)
	  {
	  case DHTLIB_OK:
	    DebugSerial.println("OK");
	    break;
	  case DHTLIB_ERROR_CHECKSUM:
	    DebugSerial.println("Checksum error");
	    break;
	  case DHTLIB_ERROR_TIMEOUT:
	    DebugSerial.println("Time out error");
	    break;
	  default:
	    DebugSerial.println("Unknown error");
	    break;
	  }

	  //发送数据到Onenet
	  postDataToOneNet(API_KEY,device_id,sensor_id1,DHT11.temperature);		
	  postDataToOneNet(API_KEY,device_id,sensor_id2,DHT11.humidity);

	 
	delay(5000);
}

int getDataFromOneNet(char* API_VALUE_temp,char* device_id_temp,char* sensor_id_temp)
{

	packet.setHostAddress(OneNetServer);
	packet.setDevId(device_id_temp);   //device_id
	packet.setAccessKey(API_VALUE_temp);  //API_KEY
	packet.setDataStreamId(sensor_id_temp);    //datastream_id


	packet.createCmdPacket(GET, TYPE_DATASTREAM);

	int httpLength = strlen(packet.content);
	

		//连接服务器
	char cmd[400];
	memset(cmd, 0, 400);	//清空cmd
	strcpy(cmd, "AT+CIPSTART=\"TCP\",\"");
	strcat(cmd, OneNetServer);
	strcat(cmd, "\",80\r\n");
	if (sendCommand(cmd, "CONNECT", 10000, 5) == Success);
	else ESP8266_ERROR(1);
	clrEsp8266RxBuffer();

	//发送数据
	memset(cmd, 0, 400);	//清空cmd
	sprintf(cmd, "AT+CIPSEND=%d\r\n", httpLength);
	if (sendCommand(cmd, ">", 3000, 1) == Success);
	else ESP8266_ERROR(2);
	clrEsp8266RxBuffer();

	memset(cmd, 0, 400);	//清空cmd
	strcpy(cmd, packet.content);
	if (sendCommand(cmd, "succ", 10000, 1) == Success);
	else ESP8266_ERROR(3);

	//解析获取的数据
	char* index1;
	char* index2;
	char getDataBuffer[10];
	memset(getDataBuffer, 0, 10);	//清空getDataBuffer

	index1 = strstr(esp8266RxBuffer, "value\":");
	index2 = strstr(index1, "},\"");
	memcpy(getDataBuffer, index1+7, index2 - (index1+7));
	DebugSerial.print("\r\nreceive get Data:\r\n==========\r\n");
	DebugSerial.print(getDataBuffer);	//输出接收到的信息	
	DebugSerial.print("\r\n==========\r\n");		

	if(strlen(getDataBuffer) == 1 && getDataBuffer[0] == '1')
		return 1;
	else if(strlen(getDataBuffer) == 1 && getDataBuffer[0] == '0')
		return 0;
	else
		return -1;

	clrEsp8266RxBuffer();

	if (sendCommand("AT+CIPCLOSE\r\n", "CLOSED", 3000, 1) == Success);
	else ESP8266_ERROR(4);
	clrEsp8266RxBuffer();
	return 0;
}

void postDataToOneNet(char* API_VALUE_temp,char* device_id_temp,char* sensor_id_temp,double thisData)
{
		//合成POST请求
	StaticJsonBuffer<200> jsonBuffer;



	JsonObject& value = jsonBuffer.createObject();
	value["value"] = thisData;

	JsonObject& id_datapoints = jsonBuffer.createObject();
	id_datapoints["id"] = sensor_id_temp;
	JsonArray& datapoints = id_datapoints.createNestedArray("datapoints");
	datapoints.add(value);

	JsonObject& myJson = jsonBuffer.createObject();
	JsonArray& datastreams = myJson.createNestedArray("datastreams");
	datastreams.add(id_datapoints);

	char p[200];
	int num = myJson.printTo(p, sizeof(p));


	packet.setHostAddress(OneNetServer);
	packet.setDevId(device_id_temp);   //device_id
	packet.setAccessKey(API_VALUE_temp);  //API_KEY
	// packet.setDataStreamId("<datastream_id>");    //datastream_id
	// packet.setTriggerId("<trigger_id>");
	// packet.setBinIdx("<bin_index>");

	/*create the http message about add datapoint */
	packet.createCmdPacket(POST, TYPE_DATAPOINT, p);
	// if (strlen(packet.content))
	// 	Serial.print(packet.content);
	// Serial.print(p);
	int httpLength = strlen(packet.content) + num;

	

		//连接服务器
	char cmd[400];
	memset(cmd, 0, 400);	//清空cmd
	strcpy(cmd, "AT+CIPSTART=\"TCP\",\"");
	strcat(cmd, OneNetServer);
	strcat(cmd, "\",80\r\n");
	if (sendCommand(cmd, "CONNECT", 10000, 5) == Success);
	else ESP8266_ERROR(1);
	clrEsp8266RxBuffer();


	//发送数据
	memset(cmd, 0, 400);	//清空cmd
	sprintf(cmd, "AT+CIPSEND=%d\r\n", httpLength);
	if (sendCommand(cmd, ">", 3000, 1) == Success);
	else ESP8266_ERROR(2);
	clrEsp8266RxBuffer();

	memset(cmd, 0, 400);	//清空cmd
	strcpy(cmd, packet.content);
	strcat(cmd, p);
	if (sendCommand(cmd, "\"succ\"}", 3000, 1) == Success);
	else ESP8266_ERROR(3);
	clrEsp8266RxBuffer();

	if (sendCommand("AT+CIPCLOSE\r\n", "CLOSED", 3000, 1) == Success);
	else ESP8266_ERROR(4);
	clrEsp8266RxBuffer();
}

void initEsp8266()
{
	if (sendCommand("AT\r\n", "OK", 1000, 10) == Success);
	else ESP8266_ERROR(5);
	clrEsp8266RxBuffer();

	// if (sendCommand("AT+RST\r\n", "ready", 5, 10000, 10) == Success);
	// else ESP8266_ERROR(6);
	//rEsp8266RxBuffer();

	if (sendCommand("AT+CWMODE=1\r\n", "OK", 3000, 10) == Success);
	else ESP8266_ERROR(7);
	clrEsp8266RxBuffer();

	char cmd[50];
	strcpy(cmd, "AT+CWJAP=\"");
	strcat(cmd, ssid);
	strcat(cmd, "\",\"");
	strcat(cmd, password);
	strcat(cmd, "\"\r\n");

	if (sendCommand(cmd, "OK", 20000, 10) == Success);
	else ESP8266_ERROR(8);
	clrEsp8266RxBuffer();

	if (sendCommand("AT+CIPMUX=0\r\n", "OK", 3000, 10) == Success);
	else ESP8266_ERROR(9);
	clrEsp8266RxBuffer();

	if (sendCommand("AT+CIFSR\r\n", "OK", 20000, 10) == Success);
	else ESP8266_ERROR(10);
	clrEsp8266RxBuffer();
}

void(* resetFunc) (void) = 0; //制造重启命令

void ESP8266_ERROR(int num)
{
	DebugSerial.print("ERROR");
	DebugSerial.println(num);
	while (1)
	{
		digitalWrite(L, HIGH);
		delay(300);
		digitalWrite(L, LOW);
		delay(300);

		if (sendCommand("AT\r\n", "OK", 100, 10) == Success)
		{
			DebugSerial.print("\r\nRESET!!!!!!\r\n");
			resetFunc();
		}
	}
}



unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry)
{
	clrEsp8266RxBuffer();
	for (unsigned char n = 0; n < Retry; n++)
	{
		DebugSerial.print("\r\nsend AT Command:\r\n----------\r\n");
		DebugSerial.write(Command);

		ESP8266Serail.write(Command);

		Time_Cont = 0;
		while (Time_Cont < Timeout)
		{
			esp8266ReadBuffer();
			if (strstr(esp8266RxBuffer, Response) != NULL)
			{
				DebugSerial.print("\r\nreceive AT Command:\r\n==========\r\n");
				DebugSerial.print(esp8266RxBuffer);	//输出接收到的信息				
				return Success;
			}
		}
		Time_Cont = 0;
	}
	DebugSerial.print("\r\nreceive AT Command:\r\n==========\r\n");
	DebugSerial.print(esp8266RxBuffer);//输出接收到的信息
	return Failure;
}



void Timer1_handler(void)
{
	Time_Cont++;
}



void esp8266ReadBuffer() {
	while (ESP8266Serail.available())
	{
		esp8266RxBuffer[ii++] = ESP8266Serail.read();
		if (ii == esp8266RxBufferLength)clrEsp8266RxBuffer();
	}
}

void clrEsp8266RxBuffer(void)
{
	memset(esp8266RxBuffer, 0, esp8266RxBufferLength);		//清空
	ii = 0;
}
