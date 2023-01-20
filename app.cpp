#include "app.h"
#include "ev3lib.h"
#include "Protocol.h"

class Button
{
public:
	Button(ev3::Function<bool()> stateFunc);
	void update();
	bool checkPush() const;
	bool checkRelease() const;
	bool pressed() const;
private:
	ev3::Function<bool()> stateFunc;
	bool curr;
	bool prev;
};

Button::Button(ev3::Function<bool()> stateFunc)
	: stateFunc(std::move(stateFunc))
{}

void Button::update()
{
	prev = curr;
	curr = stateFunc();
}

bool Button::checkPush() const
{
	return curr && !prev;
}

bool Button::checkRelease() const
{
	return !curr && prev;
}

bool Button::pressed() const
{
	return curr;
}


void main_task(intptr_t unused)
{
	const uint8_t robotAddress[6] = { 0xcc, 0x78, 0xab, 0x54, 0xd7, 0xd0 };
	const char* password = "1234";

	ev3::TouchSensor buttonGrab(ev3::SensorPort::S3);
	ev3::TouchSensor buttonPlace(ev3::SensorPort::S4);
	ev3::TouchSensor buttonEmergencyStop(ev3::SensorPort::S1);
	ev3::Motor controlMotor(ev3::MotorPort::A, ev3::MotorType::Unregulated);

	ev3::Speaker::playTone(ev3::Note::A4, 50);
	ev3::BluetoothMaster bt;
	bt.connect(robotAddress, password);
	while (!bt.connected()) { ev3::Time::delay(10); }
	ev3::Speaker::playTone(ev3::Note::A4, 50);
	ev3::Console::write("connected");

	Button buttonsMoving[] = { 
		{ []()  { return ev3::Brick::isButtonPressed(ev3::BrickButton::Up);    } },
		{ []()  { return ev3::Brick::isButtonPressed(ev3::BrickButton::Down);  } },
		{ []()  { return ev3::Brick::isButtonPressed(ev3::BrickButton::Left);  } },
		{ []()  { return ev3::Brick::isButtonPressed(ev3::BrickButton::Right); } },
	};

	Button buttonsOther[] = {
		{ [&]() { return buttonGrab.isPressed();							   } },
		{ [&]() { return buttonPlace.isPressed();							   } }
	};

	uint8_t prevPowerPercent = 100;
	while (bt.connected())
	{
		if (buttonEmergencyStop.isPressed()) 
		{
			Packet p = { Command::EmergencyStop, 0 };
			bt.writeBytes(&p, sizeof(Packet));
			ev3::Console::write("<%d %d", p.cmd, p.data);
			break; 
		}

		bool anyButtonPressed = false;
		bool anyButtonReleased = false;
		for (int i = 0; i < 4; i++)
		{
			buttonsMoving[i].update();
			anyButtonPressed = buttonsMoving[i].pressed() || anyButtonPressed;
			anyButtonReleased = buttonsMoving[i].checkRelease() || anyButtonReleased;
			if (!buttonsMoving[i].checkPush()) { continue; }
			Packet p = { (Command)i, 0 };
			bt.writeBytes(&p, sizeof(Packet));
			ev3::Console::write("<%d %d", p.cmd, p.data);
			break;
		}

		if (!anyButtonPressed && anyButtonReleased)
		{
			Packet p = { Command::Stop, 0 };
			bt.writeBytes(&p, sizeof(Packet));
			ev3::Console::write("<%d %d", p.cmd, p.data);
		}

		for (int i = 0; i < 2; i++)
		{
			buttonsOther[i].update();
			if (!buttonsOther[i].checkRelease()) { continue; }
			Packet p = { (Command)((uint8_t)Command::GrabRubbish + i), 0 };
			bt.writeBytes(&p, sizeof(Packet));
			ev3::Console::write("<%d %d", p.cmd, p.data);
			break;
		}

		uint8_t powerPercent = std::min<int>(std::max<int>(controlMotor.getCounts() / 2 + 100, 0), 200);
		if (std::abs(powerPercent - prevPowerPercent) > 9) 
		{ 
			prevPowerPercent = powerPercent;
			Packet p = { Command::SetPower, powerPercent };
			bt.writeBytes(&p, sizeof(Packet));
			ev3::Console::write("<%d %d", p.cmd, p.data);

			char num[5];
			num[4] = '\0';
			itoa(powerPercent, num, 10);
			ev3::LCD::drawRect(20, 20, 38, 28, ev3::LCDColor::White);
			ev3::LCD::drawString(num, 20, 20);

		}
	}

	ev3::Console::write("disconnected");
}
