import socket
import network
import struct
import time
import _thread
import machine
import utime
import time

ssid = "MiniBot_AP_6"
#password="MiniBot4224" #x1
#password="MiniBot8448" #x2
#password="MiniBot12672" #x3
#password="MiniBot16896" #x4
#password="MiniBot210120" #x5
password="MiniBot252144" #x6

def inet_aton(addr:str):
    return bytes(map(int, addr.split(".")))

robotStateLock = _thread.allocate_lock()


class JointStates():
    STOPPED = 'stopped'
    MOVING_CW = "moving_cw"
    MOVING_CCW = "moving_ccw"

class GripperStates():
    STOPPED = 'stopped'
    MOVING_OPEN = "moving_open"
    MOVING_CLOSED = "moving_closed"
    OPEN = "open"
    CLOSED = "closed"

class JointState():
    def __init__(self):
        self.joint_states = []
        self.joint_states.append(JointStates.STOPPED)
        self.joint_states.append(JointStates.STOPPED)
        self.joint_states.append(JointStates.STOPPED)
        self.joint_states.append(JointStates.STOPPED)        

class RobotState():
    def __init__(self):
        self.joint_state = JointState()
        self.gripper_state = GripperStates.STOPPED
        self.set_gripper_open_position = False
        self.set_gripper_closed_position = False
        self.add_waypoint = False
        self.replay = False
        self.jogging_speed = 1
        self.gripper_speed = 2
        self.replay_speed = 2.0


def parse_joint_command(command):
    print(f"Joint Command {command}")
    split_command = command.split("_")
    joint_command = split_command[0]
    print(split_command)
    joint = int(split_command[1])-1
    
    new_state = JointStates.STOPPED

    if joint_command is "mj":    
        
        direction = split_command[2]    
        
        if direction is 'cw':
            new_state = JointStates.MOVING_CW
        if direction is 'ccw':
            new_state  = JointStates.MOVING_CCW

        print(f"New State = {new_state}")
        global robotState
        robotStateLock.acquire()
        robotState.joint_state.joint_states[joint] = new_state    
        robotState.jogging_speed = int(split_command[3])
        robotStateLock.release()

    if joint_command is "sj":
        print(f"New State = {new_state}")
        global robotState
        robotStateLock.acquire()
        robotState.joint_state.joint_states[joint] = new_state    
        robotStateLock.release()


def angle_to_duty(target_angle):
    print(f"target{target_angle}")
    return int(6553/180 * target_angle + 1638)

def easeInOutQuadratic(x: float):
  return  (2 * pow(x, 2)) if (x < 0.5) else ((-2 * pow(x, 2)) + (4 * x) - 1)


def easeToPosition (servo: machine.PWM, current, target, speed=3):    
    print(f"Current {current}, Target {target}")
    if current < target:
        diff = target - current
        for x in range (0, 1000, 6):
            servo.duty_u16(current + int(easeInOutQuadratic(x/1000)*diff))
            utime.sleep_ms(speed)
    
    if current > target:
        diff = current - target
        for x in range (0, 1000, 6):
            servo.duty_u16(current - int(easeInOutQuadratic(x/1000)*diff))
            utime.sleep_ms(speed)


def multiEaseToPosition(servos: list[machine.PWM], current, target,speed=1):
    for x in range (0, 1000, 5):
        for servo in range(0, len(servos)):
            d_target = angle_to_duty(target[servo])
            d_current = angle_to_duty(current[servo])
            if d_current < d_target:
                diff = d_target - d_current
                servos[servo].duty_u16(d_current + int(easeInOutQuadratic(x/1000)*diff))
                utime.sleep_us(speed*150)
    
            if d_current >  d_target:
                diff = d_current -  d_target
                servos[servo].duty_u16(d_current - int(easeInOutQuadratic(x/1000)*diff))
                utime.sleep_us(speed*150)

def parse_gripper_movement(command):
    print(f"Gripper Movement Command {command}")
    split_command = command.split("_")

    new_state = GripperStates.STOPPED
    if split_command[1] is 'o':
        new_state = GripperStates.MOVING_OPEN
    if split_command[1] is 'c':
        new_state = GripperStates.MOVING_CLOSED

    global robotState
    robotStateLock.acquire()
    robotState.gripper_state = new_state
    robotState.gripper_speed = int(split_command[2])
    robotStateLock.release()

def parse_gripper_toggle(command):
    print(f"Gripper Toggle Command {command}")    
    split_command = command.split("_")

    new_state = GripperStates.STOPPED
    if split_command[1] is 'o':
        new_state = GripperStates.OPEN
    if split_command[1] is 'c':
        new_state = GripperStates.CLOSED

    global robotState
    robotStateLock.acquire()
    robotState.gripper_state = new_state    
    robotStateLock.release()

def parse_gripper_config(command):
    print(f"Gripper Config Command {command}")
    split_command = command.split("_")

    if split_command[1] is 'o':
        global robotState
        robotStateLock.acquire()
        robotState.set_gripper_open_position = True
        robotStateLock.release()    
    else:
        global robotState
        robotStateLock.acquire()
        robotState.set_gripper_closed_position = True
        robotStateLock.release()    
    
def parse_recording_command(command):
    print(f"Recording Command {command}")
    split_command = command.split("_")
    
    if split_command[1] is 'replay':
        global robotState
        robotStateLock.acquire()
        robotState.replay = True
        robotState.replay_speed = int(split_command[2])
        robotStateLock.release()

    if split_command[1] is 'swp':
        global robotState
        robotStateLock.acquire()
        robotState.add_waypoint = True
        robotStateLock.release()

robotState:RobotState = RobotState()

def ap_thread(ssid, password):
    print("Starting AP")
    ap = network.WLAN(network.AP_IF)
    ap.config(ssid=ssid, password = password)    
    ap.active(True)

    while ap.active() is False:
        pass

    print("AP ready, setting socket options")
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(("", 2390))
    udp_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, struct.pack(">4sI", inet_aton('224.1.1.1'),0))
    print(ap.ifconfig())
   # machine.reset()

    while True:
        data, addr = udp_socket.recvfrom(16)        
        command = str(data).split("'")[1]
        print(f"{command.find("mj")>0}")
        if command.find("mj")>=0 or command.find("sj")>=0:
            parse_joint_command(command)

        if command.find("mg") >= 0:
            parse_gripper_movement(command)
        
        if command.find("gc")>=0:
            parse_gripper_config(command)

        if command.find("gs")>=0:
            parse_gripper_toggle(command)
    
        if command.find("rc")>=0:
            parse_recording_command(command)

        print(f"Processed: {command}")


class PositionState():
    def __init__(self):
        self.joint_angles = [0,5,45,90]
        self.joint_limits = [
            (0, 180),
            (5,160),
            (0,90),
            (0,180)
        ]
        self.gripper_angle = 10
        self.gripper_state = GripperStates.OPEN
        self.hard_gripper_limits =(5,100)
        self.user_defined_limits =(5, 50)
        self.user_program = []
        self.user_program_gripper_state = []
    
    def add_waypoint(self):
        self.user_program.append([
            self.joint_angles[0],
            self.joint_angles[1],
            self.joint_angles[2],
            self.joint_angles[3]])

        self.user_program_gripper_state.append(self.gripper_state)

        print(f"Waypoints: {self.user_program} {self.user_program_gripper_state}")

    def set_gripper_open_position(self):        
        self.user_defined_limits = (self.user_defined_limits[0], self.gripper_angle)
        print(f"New gripper user limits {self.user_defined_limits}")
    
    def set_gripper_closed_position(self):        
        self.user_defined_limits = (self.gripper_angle, self.user_defined_limits[1])
        print(f"New gripper user limits {self.user_defined_limits}")

    def open_gripper(self):   
        old_value = self.gripper_angle                 
        self.gripper_angle = self.user_defined_limits[1]
        print(f"Opening gripper to {self.gripper_angle}")
        self.gripper_state= GripperStates.OPEN
        return (old_value, self.gripper_angle)
    
    def close_gripper(self):
        old_value = self.gripper_angle                 
        self.gripper_angle = self.user_defined_limits[0]
        print(f"Closing gripper to {self.gripper_angle}")
        self.gripper_state= GripperStates.CLOSED
        return (old_value, self.gripper_angle)
        
    def move_open_gripper(self, amount):
        old_value = self.gripper_angle
        max_limit = self.hard_gripper_limits[1]

        if old_value + amount <= max_limit:
            self.gripper_angle += amount
        else:
            self.gripper_angle = max_limit
        
        new_value = self.gripper_angle
        print(f"Opening gripper {amount} was {old_value} is {new_value}")
        return new_value
        
    def move_closed_gripper (self, amount):
        old_value = self.gripper_angle
        min_limit = self.hard_gripper_limits[0]

        if old_value - amount >= min_limit:
            self.gripper_angle -= amount
        else:
            self.gripper_angle = min_limit
        
        new_value = self.gripper_angle
        print(f"Opening gripper {amount} was {old_value} is {new_value}")
        return new_value

    def incr_joint(self, joint, amount):
        old_value = self.joint_angles[joint]
        max_limit =  self.joint_limits[joint][1]
        
        if old_value + amount <= max_limit:
            self.joint_angles[joint] += amount
        else:
            self.joint_angles[joint] = max_limit
        
        new_value = self.joint_angles[joint]
        print(f"Incrementing {joint} {amount} was {old_value} is {new_value}")
        return new_value

    def decr_joint(self, joint, amount):
        old_value = self.joint_angles[joint]
        min_limit = self.joint_limits[joint][0]
        
        if (old_value - amount) >= min_limit:
            self.joint_angles[joint] -= amount
        else:
            self.joint_angles[joint] = min_limit

        new_value = self.joint_angles[joint]
        print(f"Incrementing {joint} {amount} was {old_value} is {new_value}")
        return new_value

    def get_joint(self, joint):
        return self.joint_angles[joint]

def replay_program(servos, gripper, waypoints, gripper_states, positionState: PositionState, speed, gripper_speed):

    multiEaseToPosition(servos, positionState.joint_angles, waypoints[0], speed)

    for point_x in range(0, len(waypoints)-1):
        if gripper_states[point_x] is GripperStates.OPEN:
            from_to = positionState.open_gripper()
            easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),gripper_speed)
            
        if gripper_states[point_x] is GripperStates.CLOSED:
            from_to = positionState.close_gripper()
            easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),gripper_speed)
        
        multiEaseToPosition(servos, waypoints[point_x], waypoints[point_x+1], speed)

    
    positionState.joint_angles = waypoints[len(waypoints)-1]
    if gripper_states[len(waypoints)-1] is GripperStates.OPEN:
            from_to = positionState.open_gripper()
            easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),gripper_speed)
            
    if gripper_states[len(waypoints)-1] is GripperStates.CLOSED:
        from_to = positionState.close_gripper()
        easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),gripper_speed)


def movement_loop():
    joint_pins =[16,17,18,19]
    gripper_pin = 20
    positionState = PositionState()

    
    servos:list[machine.PWM] = []
    gripper = machine.PWM(machine.Pin(gripper_pin))
    gripper.freq(50)
    gripper.duty_u16(angle_to_duty(positionState.open_gripper()[1]))    
    jogging_speed = 1
    gripper_speed = 2

    for pin in range(0,4):
        print(f"Setting up servo on {pin}")
        servos.append(machine.PWM(machine.Pin(joint_pins[pin])))
        servos[pin].freq(50)        
        servos[pin].duty_u16(angle_to_duty(positionState.joint_angles[pin]))
        utime.sleep(0.5)


    while True: 
        global robotState
        robotStateLock.acquire()
        localState = robotState
        robotStateLock.release()
        # print(f"{localState.joint_state.joint_states}")
        # print(f"{localState.gripper_state.gripperState}")

    
        for joint_number in range(0,4):
            if localState.joint_state.joint_states[joint_number] is JointStates.MOVING_CCW:                
                servos[joint_number].duty_u16(angle_to_duty( positionState.incr_joint(joint_number, localState.jogging_speed)))
            
            if localState.joint_state.joint_states[joint_number] is JointStates.MOVING_CW:                
                servos[joint_number].duty_u16(angle_to_duty(positionState.decr_joint(joint_number, localState.jogging_speed)))
        
        if localState.gripper_state is GripperStates.MOVING_OPEN:            
            gripper.duty_u16(angle_to_duty(positionState.move_open_gripper(localState.gripper_speed)))

        if localState.gripper_state is GripperStates.MOVING_CLOSED:
            gripper.duty_u16(angle_to_duty(positionState.move_closed_gripper(localState.gripper_speed)))
        
        if localState.gripper_state is GripperStates.OPEN:
            from_to = positionState.open_gripper()
            easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),localState.gripper_speed)
            # gripper.duty_u16(angle_to_duty())
            global robotState
            robotStateLock.acquire()
            robotState.gripper_state = GripperStates.STOPPED
            robotStateLock.release() 

        if localState.gripper_state is GripperStates.CLOSED:
            from_to = positionState.close_gripper()
            easeToPosition(gripper, angle_to_duty(from_to[0]), angle_to_duty(from_to[1]),localState.gripper_speed)
            # gripper.duty_u16(angle_to_duty(positionState.close_gripper()))
            global robotState
            robotStateLock.acquire()
            robotState.gripper_state = GripperStates.STOPPED
            robotStateLock.release() 

        if localState.set_gripper_closed_position:
            positionState.set_gripper_closed_position()
            global robotState
            robotStateLock.acquire()
            robotState.set_gripper_closed_position = False
            robotStateLock.release() 
        
        if localState.set_gripper_open_position:
            positionState.set_gripper_open_position()
            global robotState
            robotStateLock.acquire()
            robotState.set_gripper_open_position = False
            robotStateLock.release() 
        
        if localState.add_waypoint:
            positionState.add_waypoint()
            global robotState
            robotStateLock.acquire()
            robotState.add_waypoint = False
            robotStateLock.release() 
        
        if localState.replay:
            replay_program(
                servos, 
                gripper, 
                positionState.user_program, 
                positionState.user_program_gripper_state, 
                positionState, 
                localState.replay_speed, 
                localState.gripper_speed)            
            global robotState
            robotStateLock.acquire()
            robotState.replay = False
            robotStateLock.release() 



        
        utime.sleep(0.02)



# _thread.start_new_thread(printer, ())
_thread.start_new_thread(movement_loop,())


ap_thread(ssid = ssid,password= password)


