#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <myo/myo.hpp>

using namespace System;
using namespace System::IO::Ports;

class DataCollector : public myo::DeviceListener {

public:
    DataCollector()
    : onArm(false), isUnlocked(false), roll_w(0), pitch_w(0), yaw_w(0), currentPose()
    {
    }

    void onUnpair(myo::Myo* myo, uint64_t timestamp)
    {
	// Clean up
        roll_w = 0;
        pitch_w = 0;
        yaw_w = 0;
        onArm = false;
        isUnlocked = false;
    }

    void onOrientationData(myo::Myo* myo, uint64_t timestamp, const myo::Quaternion<float>& quat)
    {
        using std::atan2;
        using std::asin;
        using std::sqrt;
        using std::max;
        using std::min;

        float roll = atan2(2.0f * (quat.w() * quat.x() + quat.y() * quat.z()),
                           1.0f - 2.0f * (quat.x() * quat.x() + quat.y() * quat.y()));
        float pitch = asin(max(-1.0f, min(1.0f, 2.0f * (quat.w() * quat.y() - quat.z() * quat.x()))));
        float yaw = atan2(2.0f * (quat.w() * quat.z() + quat.x() * quat.y()),
                        1.0f - 2.0f * (quat.y() * quat.y() + quat.z() * quat.z()));

        roll_w = static_cast<int>((roll + (float)M_PI)/(M_PI * 2.0f) * 18);
        pitch_w = static_cast<int>((pitch + (float)M_PI/2.0f)/M_PI * 18);
        yaw_w = static_cast<int>((yaw + (float)M_PI)/(M_PI * 2.0f) * 18);
    }

    void onPose(myo::Myo* myo, uint64_t timestamp, myo::Pose pose)
    {
        currentPose = pose;

        if (pose != myo::Pose::unknown && pose != myo::Pose::rest) {
            myo->unlock(myo::Myo::unlockHold);
			myo->notifyUserAction();
			std::string poseString = currentPose.toString();
			std::cout << poseString << std::endl;
        } else {
            myo->unlock(myo::Myo::unlockTimed); // Inactive
        }
    }

    void onArmSync(myo::Myo* myo, uint64_t timestamp, myo::Arm arm, myo::XDirection xDirection)
    {
        onArm = true;
        whichArm = arm;
    }

    void onArmUnsync(myo::Myo* myo, uint64_t timestamp)
    {
        onArm = false;
    }

    void onUnlock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = true;
		rotateComplete = false;
    }

    void onLock(myo::Myo* myo, uint64_t timestamp)
    {
        isUnlocked = false;
		rotateComplete = true;
    }
	
	void sendToArduino( SerialPort^ arduino )
    {
        if (onArm) {
            
            std::string poseString = currentPose.toString();

			if (poseString == "fist")
			{
				if (firstRun == true)
				{
					startRotate = roll_w;
					firstRun = false;
				}
				if (rotateComplete == false)
				{
					if (roll_w >= (1.2 * startRotate))
					{
						std::cout << "Rotation Done" << std::endl;
						arduino->WriteLine("1");
						rotateComplete = true;
					}
					else if (roll_w <= (startRotate/1.2))
					{
						std::cout << "Rotation Done" << std::endl;
						arduino->WriteLine("0");
						rotateComplete = true;
					}
				}
			}
			else
			{
				firstRun = true;
				rotateComplete = false;
			}
        } 
    }

    bool onArm;
    myo::Arm whichArm;
    bool isUnlocked;
    int roll_w, pitch_w, yaw_w;
    myo::Pose currentPose;
	std::string poseString;
	bool firstRun;
	bool rotateComplete;
	int startRotate;
};

int main(int argc, char** argv)
{
	// Require CLI for Serial Comm?
	String^ portName;
	int baudRate=9600;
	portName = "Com6"; // Bluetooth Port
	SerialPort^ arduino;
	arduino = gcnew SerialPort(portName, baudRate);

    try 
	{
		arduino->Open();

		myo::Hub hub("com.myo.door.lock");

		std::cout << "Attempting to find a device." << std::endl;

		myo::Myo* myo = hub.waitForMyo(10000);

		if (!myo) 
		{
		    throw std::runtime_error("Unable to find a device.");
		}

		std::cout << "Connected." << std::endl << std::endl;

		DataCollector collector;

		hub.addListener(&collector);

		while (1) 
		{
			hub.run(1000/20);  // 20 times a second

			collector.sendToArduino(arduino);
		}

		arduino->Close();
	} 
	catch (const std::exception& e) 
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::cerr << "Press enter to continue.";
		std::cin.ignore();
		return 1;
	}	
	catch (IO::IOException^ e ) 
	{ 
		std::cerr << e->GetType()->Name << ": Port is not ready";
	}	
}
