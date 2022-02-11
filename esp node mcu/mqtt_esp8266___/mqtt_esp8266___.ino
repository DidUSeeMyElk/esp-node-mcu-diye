/*
基本ESP8266 MQTT示例
此草图演示了pubsub库与ESP8266板/库的结合能力。
使用ESP8266板/库.
它连接到MQTT服务器，然后：
-每一秒钟将“hello world”发布到主题“outTopic”
-订阅主题“inTopic”，打印它收到的任何消息。注意-它假定接收到的有效负载是字符串而不是二进制
-如果主题“inTopic”的第一个字符为1，则打开ESP Led，否则将其关闭
如果使用阻塞重新连接功能丢失连接，它将重新连接到服务器。请参阅“mqtt_reconnect_nonblocking”示例，了解如何在不阻塞主回路的情况下实现相同的结果。
要安装ESP8266板（使用Arduino 1.6.4+）：
-在“文件->首选项->其他线路板管理器URL”下添加以下第三方线路板管理器：
http://arduino.esp8266.com/stable/package_esp8266com_index.json
-打开“工具->线路板->线路板管理器”，然后单击ESP8266的安装
-在“工具->线路板”中选择您的ESP8266
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>

//下面为各类参数定义，ssid为wifi名字，password为wifi密码，mqtt_server为mqtt服务器IPV4地址，mqttPort为服务器端口，Baudrate为通信波特率

const char* ssid = "Elk";
const char* password = "knightstar";
const char* mqtt_server = "121.4.157.120";
const int mqttPort = 1883;
const int Baudrate = 115200;
//const char* mqtt_pubTopic = "outTopic";
const char* mqtt_subTopic = "inTopic";
//const char* mqtt_pubMessge = "Hello World";



#define DropLength  15        //液滴记录数组深度，一项为记录一秒内液滴滴落数
#define callingcheck  5        //呼叫判断时间，几秒内无液滴落下报警设置
unsigned int callingmode=0;     //呼叫变量，1为滴液完成，呼叫后端，0为未完成
unsigned int dropNumber=0;        //滴液总滴数
unsigned int dropPre=0;         //1s前液滴总滴数，用于观察是否1s内液滴滴落数
unsigned int timer0timer=0;      //计时器内计时数组，用于记录对 液滴记录数组 的第几位数进行赋值
unsigned int secDrop[DropLength];     //液滴记录数组      
unsigned int v=0;            //滴液速度
unsigned int TIME=0;          //总时间
#define key_PIN D2 //D2为中断口，即IO4

WiFiClient espClient;
PubSubClient client(espClient);
Ticker ticker;

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() 
{

  delay(10);
  //从连接wifi开始
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);//esp8266设定为statoin模式
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }//等待连接上wifi

 // randomSeed(micros());//伪随机数生成器，micros函数返回自程序启动后的微秒数

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length)//接收响应函数
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //如果接收到1作为第一个字符，则打开LED
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   //打开LED（注意此时低电平，但实际上LED是亮着的；这是因为它在ESP-01上处于低激活状态）
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  //通过高电平来关闭LED
  }

}

void reconnect() 
{
  //一直循环，直到重新连接
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";   //创建一个随机的客户端ID
    clientId += String(random(0xffff), HEX);
    //尝试连接
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
//      client.publish(mqtt_pubTopic,mqtt_pubMessge);    //一旦连接后，发布信息
      client.subscribe(mqtt_subTopic);//并且订阅标题
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);    //等待5秒钟后重试
    }
  }
}

ICACHE_RAM_ATTR void Interrupt() //外部中断响应函数
{  
  dropNumber++;
  callingmode=0;
}

void secDropInit()             //初始化数组，为了避免其在编译器下预编译皆为0
{
  unsigned char i=0;
  for(;i<DropLength;i++)
  secDrop[i]=i;
}

void Timer0()
{
    unsigned int i=0;
    unsigned int middle=0;
    unsigned int l=0;
    unsigned int k=0;
    unsigned int CheckPoint=0;
    timer0timer++;      //计时器计数位+1
    TIME++;                    //总时间+1s
    if(TIME<DropLength) 
    {
      secDrop[timer0timer-1]=dropNumber-dropPre;  //计算这一秒内落下的液滴，放入当前的计数器计数位
      dropPre=dropNumber;             //记录下当前液滴落下的个数
      for(i=0;i<TIME;i++)
      middle+=secDrop[i];             //计算近15s内总液滴数，中间变量middle对数组进行累加
      v=60*middle/TIME;             //计算近15s内液滴下降速度，单位为滴每分钟
      middle=0;                 //中间变量置零
    }
    else
    {
      secDrop[timer0timer-1]=dropNumber-dropPre;  //计算这一秒内落下的液滴，放入当前的计数器计数位
      if(timer0timer==DropLength)    //如果计时器计数位到达了15，就从头开始
      timer0timer=0;
      dropPre=dropNumber;             //记录下当前液滴落下的个数
      for(i=0;i<DropLength;i++)
      middle+=secDrop[i];              //计算近15s内总液滴数，中间变量middle对数组进行累加
      v=4*middle;                  //计算近15s内液滴下降速度，单位为滴每分钟
      middle=0;                  //中间变量置零
    }
    k=0;                     //呼叫判断变量置零
    if(timer0timer<callingcheck)               //如果定时器计时变量小于呼叫检测
    {
              //检查点就设在滴液记录数组尾部
      for(CheckPoint=DropLength-callingcheck+timer0timer;CheckPoint<DropLength;CheckPoint++)          //先进行验证尾部是否无滴液落下
      {
        if(secDrop[CheckPoint]!=0)                 //如果有滴液则直接跳出循环
        break;                           
        else                           //如果没有，k自增
        k++;
      }
      for(l=0;l<timer0timer;l++)                   //再检查头部是否无滴液落下
      {
        if(secDrop[l]!=0)                    //如果有滴液则直接跳出循环
        break;
        else                          //如果没有，k自增
        k++;  
      }
    }
    else                               //如果定时器计时变量大于等于呼叫检测
    {
                   //检查点可直接减一个呼叫检查周期得到
      for(CheckPoint=timer0timer-callingcheck;CheckPoint<timer0timer;CheckPoint++)
      {
        if(secDrop[CheckPoint]!=0)                 //如果有滴液则直接跳出循环
        break;
        else
        k++;                          //如果没有，k自增
      }
    }
    if(k==callingcheck)                        //检查k是否达到了呼叫检测所需要的时间
    callingmode=1;                           //如果是，呼叫变量置1 
 //   client.publish("v",v,8);
//    client.publish("minute",TIME/60);
//    client.publish("second",TIME%60);
//    client.publish("callingmode",callingmode);
      snprintf (msg, MSG_BUFFER_SIZE, "%ld", v);
      client.publish("v",msg);
      snprintf (msg, MSG_BUFFER_SIZE, "%ld", TIME/60);
      client.publish("minute",msg);
      snprintf (msg, MSG_BUFFER_SIZE, "%ld", TIME%60);
      client.publish("second",msg);
      snprintf (msg, MSG_BUFFER_SIZE, "%ld", callingmode);
      client.publish("callingmode",msg);


      
    
    
}
void setup() 
{
  ticker.attach(1, Timer0);
  pinMode(BUILTIN_LED, OUTPUT);     //将内置LED引脚初始化为输出
  pinMode(key_PIN,INPUT_PULLUP);
  attachInterrupt(key_PIN, Interrupt, FALLING);
  secDropInit();
  Serial.begin(Baudrate);         //初始化波特率
  setup_wifi();             //设置esp8266的wifi设置
  client.setServer(mqtt_server, mqttPort);    //设置mqtt服务器内容
  client.setCallback(callback);       //设置mqtt接收回应
}

void loop() 
{

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

//  unsigned long now = millis();
/*  if (now - lastMsg > 1000)   
 {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "Hello World #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
  }
*/
}
