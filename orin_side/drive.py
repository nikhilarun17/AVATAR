from AVATAR import AVATARlink
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32MultiArray, Bool

pwmlist = [0, 0, 0, 0, 0, 0]
state = 0
node = None

LAYOUT = {
    "pwm1": 2,
    "pwm2": 3,
    "pwm3": 4,
    "pwm4": 5,
    "pwm5": 6,
    "pwm6": 7,
    "state": 8,
    "pwmecho1": 12,
    "pwmecho2": 13,
    "pwmecho3": 14,
    "pwmecho4": 15,
    "pwmecho5": 16,
    "pwmecho6": 17,
    "stateecho": 18
}

link = AVATARlink(port="/dev/ttyACM0", layout=LAYOUT)


class DriveComNode(Node):
    def __init__(self):
        super().__init__('drive_com_node')

        self.pwmsub = self.create_subscription(Int32MultiArray, 'drive_pwm', self.pwmcallback, 10)
        self.statesub = self.create_subscription(Bool, 'state', self.statecallback, 10)

        self.pwmechopub = self.create_publisher(Int32MultiArray, '/drive_pwm_echo', 10)
        self.stateechopub = self.create_publisher(Bool, '/state_echo', 10)

        self.get_logger().info("AVATAR drive bridge ready.")

    def pwmcallback(self, msg):
        global pwmlist
        n = min(len(msg.data), 6)
        for i in range(n):
            pwmlist[i] = int(msg.data[i])
        link.send_fields({f"pwm{i+1}": pwmlist[i] for i in range(6)})

    def statecallback(self, msg):
        global state
        state = 1 if msg.data else 0
        link.send_value('state', state)


def on_packet(fields):
    if node is None:
        return
    if "pwmecho1" in fields:
        echo_msg = Int32MultiArray()
        echo_msg.data = [int(fields.get(f"pwmecho{i}", 0)) for i in range(1, 7)]
        node.pwmechopub.publish(echo_msg)


link.packet_callback = on_packet


def main():
    global node
    rclpy.init()
    node = DriveComNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
    link.close()


if __name__ == '__main__':
    main()