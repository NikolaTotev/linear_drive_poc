import machine
import time
servoPin = 20
servo = machine.PWM(machine.Pin(servoPin))
servo.freq(50)

while True:
    angle = int(input('Enter Angle'))
    writeVal=6553/180 * angle +1638
    servo.duty_u16(int(writeVal))
    time.sleep(0.02)