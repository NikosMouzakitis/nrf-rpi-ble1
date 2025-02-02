import threading

import struct

import matplotlib.pyplot as plt

import matplotlib.animation as animation

from collections import deque

from bluepy.btle import Peripheral, UUID, ADDR_TYPE_RANDOM, DefaultDelegate



# Define a delegate to handle notifications

class NotificationDelegate(DefaultDelegate):
    def __init__(self, data_queue):
        DefaultDelegate.__init__(self)
        self.data_queue = data_queue
    def handleNotification(self, cHandle, data):
        if len(data) == 8:  # Ensure data is 8 bytes (double precision)
            value = struct.unpack('<d', data)[0]
            print(f"Received data: {value}")
            self.data_queue.append(value)  # Add value to queue

# Function to update the plot dynamically

def update_plot(frame, data_queue, line):
    line.set_ydata(data_queue)
    line.set_xdata(range(len(data_queue)))
    ax.relim()  # Recalculate limits
    ax.autoscale_view(True, True, True)  # Autoscale the view
    return line,



# Function to handle BLE connection and notifications

def ble_listener():

    global running

    try:

        print(f"Connecting to {nrf_address}...")
        peripheral = Peripheral(nrf_address, addrType=ADDR_TYPE_RANDOM)
        print("Connected!")
        # Set delegate
        peripheral.setDelegate(NotificationDelegate(data_queue))



        # Enable notifications on the custom characteristic
        print("Enabling notifications...")
        peripheral.writeCharacteristic(cccd_handle, b"\x01\x00", withResponse=True)

        print("Waiting for notifications...")

        while running:  # Keep running until user stops

            if peripheral.waitForNotifications(1.0):  

                continue  # Notification received, continue listening

            print("Waiting...")



    except Exception as e:

        print(f"Error: {e}")

    finally:

        peripheral.disconnect()

        print("Disconnected.")



#update each time upon the address.
nrf_address = "6C:91:36:90:97:D7"

custom_char_handle = 0x0012  

cccd_handle = custom_char_handle + 1  



# Queue to store received data

data_queue = deque(maxlen=100)  # Store up to 100 values



# Set up the plot

fig, ax = plt.subplots()

line, = ax.plot([], [], 'b-')  # Initialize an empty line

ax.set_xlabel('Time')

ax.set_ylabel('Value')

ax.set_title('Real-Time Data from nRF52840')

ax.grid(True)




ani = animation.FuncAnimation(fig, update_plot, fargs=(data_queue, line), interval=100)



# Start BLE listener in a separate thread

running = True

ble_thread = threading.Thread(target=ble_listener, daemon=True)

ble_thread.start()



try:

    plt.show()  # Show plot, keep updating

except KeyboardInterrupt:

    running = False  # Stop the BLE thread if user interrupts



running = False  # Stop the BLE thread when plot is closed

ble_thread.join()


