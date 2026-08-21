[ComponentEditorProps(category: "GameScripted/PropFlightCore", description: "Reads pilot inputs from CarContext, exposes to FlightModel. Owner pushes smoothed inputs to server via RPC.")]
class PFC_FlightControllerClass : ScriptGameComponentClass
{
}

class PFC_FlightController : ScriptGameComponent
{
	[Attribute("0.5", UIWidgets.EditBox, "Throttle change rate per second.")]
	protected float m_fThrottleRate;

	[Attribute("1.0", UIWidgets.EditBox, "Roll input slew rate (units/sec). 1.0 = full deflection in 1s.")]
	protected float m_fControlRate;

	[Attribute("0.7", UIWidgets.EditBox, "Pitch input slew rate (units/sec). Slower than roll — heavy elevator.")]
	protected float m_fPitchControlRate;

	[Attribute("1.5", UIWidgets.EditBox, "Yaw input slew rate (units/sec). Fastest — rudder pedals are quick.")]
	protected float m_fYawControlRate;

	[Attribute("0.7", UIWidgets.EditBox, "Max pitch input magnitude (0..1). Caps elevator authority.")]
	protected float m_fPitchInputScale;

	[Attribute("-1.0", UIWidgets.EditBox, "Nose-wheel ground steering scale. The smoothed Airplane_Yaw (Q/E) input is written into the native CarSteering action so the wheel sim steers the nose wheel, freeing A/D to roll only. Sign matches rudder convention (Q=left, E=right); flip to invert; 0 disables (A/D steers as vanilla).")]
	protected float m_fGroundSteerScale;

	[Attribute("16", UIWidgets.EditBox, "Milliseconds between input RPCs to server.")]
	protected float m_fInputSendIntervalMs;

	[Attribute("0", UIWidgets.CheckBox, "Persistent pitch: W/S moves the virtual yoke and releasing HOLDS it there; opposite input returns it. Removes keyboard yoyo. Pitch slew-rate curves are bypassed in this mode. Applies to digital (key) input only - analog devices (gamepad/joystick/mouse axes) keep the normal spring-back behavior.")]
	protected bool m_bPersistentPitch;

	[Attribute("2.5", UIWidgets.EditBox, "Persistent pitch: seconds of held input for full yoke travel (center to full deflection).")]
	protected float m_fPersistentPitchSeconds;

	[Attribute("0.04", UIWidgets.EditBox, "Mouse aircraft control: pitch stick deflection per unit of Airplane_MousePitch. Used while the vanilla 'Mouse aircraft control' gameplay setting is on; mouse position becomes a persistent virtual stick, hold/tap Freelook (Alt) to look around. Negative inverts.")]
	protected float m_fMouseFlightPitchSens;

	[Attribute("0.04", UIWidgets.EditBox, "Mouse aircraft control: roll stick deflection per unit of Airplane_MouseRoll. Negative inverts.")]
	protected float m_fMouseFlightRollSens;

	[Attribute("2.5", UIWidgets.EditBox, "Mouse aircraft control: slew rate (units/sec) of the published input chasing the mouse stick, same for pitch and roll so the pilot hand moves uniformly. High-IAS pitch slew falloff still caps pitch on top.")]
	protected float m_fMouseStickSlewRate;

	[Attribute("120", UIWidgets.EditBox, "Mouse aircraft control: head-aim return-to-center rate (deg/s) while the mouse is flying the plane. 0 leaves the view where freelook left it.")]
	protected float m_fMouseLookReturnRate;

	protected IEntity m_Owner;
	protected InputManager m_InputMgr;
	protected SCR_BaseCompartmentManagerComponent m_CompartmentMgr;
	protected RplComponent m_RplComponent;
	protected bool m_bLocalPilotActive;

	protected float m_fPitchInput;
	protected float m_fRollInput;
	protected float m_fYawInput;
	protected float m_fThrottle;

	protected float m_fLastInputSendMs = -1e9;

	protected bool m_bThrottleAxisEngaged;
	protected float m_fThrottleAxisRef = -999;

	// Latched device class of the last active pitch input: analog devices spring back
	// even in persistent mode, so releasing a gamepad/joystick/mouse axis recenters
	// while releasing a key holds. Only updated while input is nonzero — at zero the
	// latched mode decides between hold (digital) and recenter (analog).
	protected bool m_bPitchInputAnalog;

	protected float m_fMouseStickPitch;
	protected float m_fMouseStickRoll;
	protected float m_fMouseDbgPitch;
	protected float m_fMouseDbgRoll;
	protected bool m_bMouseFlightWasActive;
	protected bool m_bMouseFreelookLatch;
	protected bool m_bFreelookTogglePrev;
	protected IEntity m_HeadAimingChar;
	protected CharacterHeadAimingComponent m_HeadAiming;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		m_InputMgr = GetGame().GetInputManager();
		m_CompartmentMgr = SCR_BaseCompartmentManagerComponent.Cast(owner.FindComponent(SCR_BaseCompartmentManagerComponent));
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		EventHandlerManagerComponent ev = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (ev)
		{
			ev.RegisterScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered, false);
			ev.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft, false);
		}
	}

	override void OnDelete(IEntity owner)
	{
		EventHandlerManagerComponent ev = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (ev)
		{
			ev.RemoveScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered);
			ev.RemoveScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
		}
		super.OnDelete(owner);
	}

	// Active-pilot state is derived from the engine's IsPiloting() each tick (see PollInput), not seat
	// occupancy — a seated copilot who hasn't taken controls (SetActivePilotCompartment) produces no input.
	protected void OnCompartmentEntered(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
	}

	protected void OnCompartmentLeft(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		if (!IsPilotSlot(mgr, slotID, managerId))
			return;
		if (occupant != SCR_PlayerController.GetLocalControlledEntity())
			return;
		m_bLocalPilotActive = false;
		m_fPitchInput = 0;
		m_fRollInput = 0;
		m_fYawInput = 0;
		m_fMouseStickPitch = 0;
		m_fMouseStickRoll = 0;
		m_bMouseFlightWasActive = false;
		m_bMouseFreelookLatch = false;
	}

	protected bool IsPilotSlot(BaseCompartmentManagerComponent mgr, int slotID, int managerId)
	{
		if (!mgr)
			return false;
		BaseCompartmentSlot slot = mgr.FindCompartment(slotID, managerId);
		return PilotCompartmentSlot.Cast(slot) != null;
	}

	protected bool LocalIsPiloting()
	{
		return PFC_PilotUtil.IsLocalActivePilot(m_Owner);
	}

	void PollInput(float timeSlice)
	{
		if (m_RplComponent && !m_RplComponent.IsOwner())
			return;

		m_bLocalPilotActive = LocalIsPiloting();

		float rollMaxDelta  = m_fControlRate * timeSlice;
		float yawMaxDelta   = m_fYawControlRate * timeSlice;

		if (!m_bLocalPilotActive || !m_InputMgr)
		{
			m_fPitchInput = MoveTowards(m_fPitchInput, 0, GetPitchSlewRate(m_fPitchInput, 0) * timeSlice);
			m_fRollInput  = MoveTowards(m_fRollInput,  0, rollMaxDelta);
			m_fYawInput   = MoveTowards(m_fYawInput,   0, yawMaxDelta);
			m_bMouseFlightWasActive = false;
		}
		else
		{
			float pitch = 0;
			float roll = 0;
			float yaw = 0;
			if (!PFC_PilotUtil.IsFlightInputSuppressed("Airplane_Pitch"))
				pitch = m_InputMgr.GetActionValue("Airplane_Pitch");
			if (!PFC_PilotUtil.IsFlightInputSuppressed("Airplane_Roll"))
				roll = m_InputMgr.GetActionValue("Airplane_Roll");
			if (!PFC_PilotUtil.IsFlightInputSuppressed("Airplane_Yaw"))
				yaw = m_InputMgr.GetActionValue("Airplane_Yaw");

			bool mouseFlight = IsMouseFlightActive();
			if (mouseFlight && !m_bMouseFlightWasActive)
			{
				m_fMouseStickPitch = m_fPitchInput;
				m_fMouseStickRoll = m_fRollInput;
			}
			m_bMouseFlightWasActive = mouseFlight;

			if (mouseFlight)
			{
				// keys nudge the same virtual stick the mouse holds (heli cyclic behavior)
				float keyTravel = timeSlice / Math.Max(m_fPersistentPitchSeconds, 0.1);
				m_fMouseStickPitch = Math.Clamp(m_fMouseStickPitch + Math.Clamp(pitch, -1, 1) * keyTravel * m_fPitchInputScale, -m_fPitchInputScale, m_fPitchInputScale);
				m_fMouseStickRoll = Math.Clamp(m_fMouseStickRoll + Math.Clamp(roll, -1, 1) * m_fControlRate * timeSlice, -1, 1);
				// slew toward the mouse stick so surfaces (and the pilot hand anim) move at a bounded,
				// per-axis-uniform rate; the variant high-IAS pitch falloff still caps pitch on top
				float mousePitchRate = Math.Min(m_fMouseStickSlewRate, GetPitchSlewRate(m_fPitchInput, m_fMouseStickPitch));
				m_fPitchInput = MoveTowards(m_fPitchInput, m_fMouseStickPitch, mousePitchRate * timeSlice);
				m_fRollInput = MoveTowards(m_fRollInput, m_fMouseStickRoll, m_fMouseStickSlewRate * timeSlice);
			}
			else
			{
				if (Math.AbsFloat(pitch) > 0.01)
				{
					// Fractional values can only come from an analog device — key filters yield exact
					// 0/±1 — so partial stick deflection never reaches the persistent integrator even
					// if the reported input type is wrong.
					float mag = Math.AbsFloat(pitch);
					bool fractional = (mag - Math.Floor(mag)) > 0.02 && (Math.Ceil(mag) - mag) > 0.02;
					m_bPitchInputAnalog = fractional || m_InputMgr.GetActionInputType("Airplane_Pitch") != EActionValueType.DIGITAL;
				}

				if (m_bPersistentPitch && !m_bPitchInputAnalog)
				{
					float travel = timeSlice / Math.Max(m_fPersistentPitchSeconds, 0.1);
					float cap = m_fPitchInputScale;
					m_fPitchInput = Math.Clamp(m_fPitchInput + Math.Clamp(pitch, -1, 1) * travel * cap, -cap, cap);
				}
				else
				{
					float pitchTarget = Math.Clamp(pitch, -1, 1) * m_fPitchInputScale;
					m_fPitchInput = MoveTowards(m_fPitchInput, pitchTarget, GetPitchSlewRate(m_fPitchInput, pitchTarget) * timeSlice);
				}

				m_fRollInput = MoveTowards(m_fRollInput, Math.Clamp(roll, -1, 1), rollMaxDelta);
			}

			m_fYawInput = MoveTowards(m_fYawInput, Math.Clamp(yaw, -1, 1), yawMaxDelta);

			bool axisSuppressed = PFC_PilotUtil.IsFlightInputSuppressed("Airplane_ThrottleAxis");

			float thrAxis = 0;
			float thrUp = 0;
			float thrDown = 0;
			if (!axisSuppressed)
				thrAxis = m_InputMgr.GetActionValue("Airplane_ThrottleAxis");
			if (!PFC_PilotUtil.IsFlightInputSuppressed("Airplane_ThrottleUp"))
				thrUp = m_InputMgr.GetActionValue("Airplane_ThrottleUp");
			if (!PFC_PilotUtil.IsFlightInputSuppressed("Airplane_ThrottleDown"))
				thrDown = m_InputMgr.GetActionValue("Airplane_ThrottleDown");

			if (m_bThrottleAxisEngaged && (thrUp > 0.5 || thrDown > 0.5))
			{
				m_bThrottleAxisEngaged = false;
				m_fThrottleAxisRef = thrAxis;
			}

			if (!m_bThrottleAxisEngaged && !axisSuppressed)
			{
				if (m_fThrottleAxisRef < -900)
					m_fThrottleAxisRef = thrAxis;
				else if (Math.AbsFloat(thrAxis - m_fThrottleAxisRef) > 0.05)
					m_bThrottleAxisEngaged = true;
			}

			if (m_bThrottleAxisEngaged)
			{
				if (!axisSuppressed)
					m_fThrottle = Math.Clamp(thrAxis, 0, 1);
			}
			else
			{
				if (thrUp > 0.5)
					m_fThrottle = Math.Clamp(m_fThrottle + timeSlice * m_fThrottleRate, 0, 1);
				if (thrDown > 0.5)
					m_fThrottle = Math.Clamp(m_fThrottle - timeSlice * m_fThrottleRate, 0, 1);
			}

			if (DiagMenu.GetBool(SCR_DebugMenuID.PFC_DEBUG_INPUT))
				DrawInputDebug(pitch, roll, yaw, thrAxis);
		}

		if (!Replication.IsServer() && m_RplComponent && m_RplComponent.Id().IsValid())
		{
			float now = System.GetTickCount();
			if (now - m_fLastInputSendMs >= m_fInputSendIntervalMs)
			{
				Rpc(RpcSrv_ReceiveInputs, m_fPitchInput, m_fRollInput, m_fYawInput, m_fThrottle);
				m_fLastInputSendMs = now;
			}
		}
	}

	bool IsMouseFlightActive()
	{
		if (!m_bLocalPilotActive || !m_InputMgr)
			return false;
		if (!m_InputMgr.IsUsingMouseAndKeyboard())
			return false;
		if (!CharacterControllerComponent.GetMouseControlAircraft())
			return false;
		// MouseXRel/MouseYRel are context-free — without this the map/menu cursor would fly the plane
		MenuManager menus = GetGame().GetMenuManager();
		if (menus && (menus.IsAnyMenuOpen() || menus.IsAnyDialogOpen()))
			return false;
		SCR_MapEntity mapEnt = SCR_MapEntity.GetMapInstance();
		if (mapEnt && mapEnt.IsOpen())
			return false;
		return !IsMouseFlightSuppressed();
	}

	// Variant hook (JetFlightCore autopilot etc.): true releases the mouse back to freelook.
	protected bool IsMouseFlightSuppressed()
	{
		return false;
	}

	bool IsMouseFreelookActive()
	{
		if (m_bMouseFreelookLatch)
			return true;
		return m_InputMgr.GetActionValue("Freelook") > 0.1;
	}

	// Per-frame (NOT per sim step — MouseXRel/MouseYRel are per-frame motion values,
	// physics substeps would integrate the same delta twice). Called from EOnFrame.
	void UpdateMouseFlight(float timeSlice)
	{
		if (m_RplComponent && !m_RplComponent.IsOwner())
			return;
		if (!IsMouseFlightActive())
		{
			m_bMouseFreelookLatch = false;
			m_bFreelookTogglePrev = false;
			m_bMouseFlightWasActive = false;
			return;
		}

		bool toggle = m_InputMgr.GetActionValue("FreelookToggle") > 0.5;
		if (toggle && !m_bFreelookTogglePrev)
			m_bMouseFreelookLatch = !m_bMouseFreelookLatch;
		m_bFreelookTogglePrev = toggle;

		if (IsMouseFreelookActive())
			return;

		// AnalogRelative actions report the per-frame mouse delta (ManualCameraRotateYaw pattern)
		float deltaPitch = m_InputMgr.GetActionValue("Airplane_MousePitch");
		float deltaRoll = m_InputMgr.GetActionValue("Airplane_MouseRoll");
		m_fMouseDbgPitch = deltaPitch;
		m_fMouseDbgRoll = deltaRoll;

		m_fMouseStickPitch = Math.Clamp(m_fMouseStickPitch + deltaPitch * m_fMouseFlightPitchSens, -m_fPitchInputScale, m_fPitchInputScale);
		m_fMouseStickRoll = Math.Clamp(m_fMouseStickRoll + deltaRoll * m_fMouseFlightRollSens, -1, 1);

		m_InputMgr.ResetAction("MouseX");
		m_InputMgr.SetActionValue("MouseX", 0);
		m_InputMgr.ResetAction("MouseY");
		m_InputMgr.SetActionValue("MouseY", 0);

		RecenterHeadAim(timeSlice);
	}

	protected void RecenterHeadAim(float timeSlice)
	{
		if (m_fMouseLookReturnRate <= 0.01)
			return;
		IEntity ch = SCR_PlayerController.GetLocalControlledEntity();
		if (ch != m_HeadAimingChar)
		{
			m_HeadAimingChar = ch;
			m_HeadAiming = null;
			if (ch)
				m_HeadAiming = CharacterHeadAimingComponent.Cast(ch.FindComponent(CharacterHeadAimingComponent));
		}
		if (!m_HeadAiming)
			return;
		vector rot = m_HeadAiming.GetAimingRotationWanted();
		float maxDelta = m_fMouseLookReturnRate * timeSlice;
		rot[0] = MoveTowards(rot[0], 0, maxDelta);
		rot[1] = MoveTowards(rot[1], 0, maxDelta);
		m_HeadAiming.SetAimingRotationWanted(rot);
	}

	protected void DrawInputDebug(float pitch, float roll, float yaw, float thrAxis)
	{
		DbgUI.Begin("Flight Input");
		DbgUI.Text(string.Format("pitch raw:  %1   type: %2", pitch.ToString(5, 3), InputTypeName(m_InputMgr.GetActionInputType("Airplane_Pitch"))));
		DbgUI.Text(string.Format("pitch out:  %1   latch: %2", m_fPitchInput.ToString(5, 3), AnalogLatchName()));
		DbgUI.Text(string.Format("roll raw:   %1   type: %2", roll.ToString(5, 3), InputTypeName(m_InputMgr.GetActionInputType("Airplane_Roll"))));
		DbgUI.Text(string.Format("roll out:   %1", m_fRollInput.ToString(5, 3)));
		DbgUI.Text(string.Format("yaw raw:    %1", yaw.ToString(5, 3)));
		DbgUI.Text(string.Format("yaw out:    %1", m_fYawInput.ToString(5, 3)));
		DbgUI.Text(string.Format("thr axis:   %1   throttle: %2", thrAxis.ToString(5, 3), m_fThrottle.ToString(5, 3)));
		if (IsMouseFlightActive())
		{
			DbgUI.Text(string.Format("mouse stick: %1 / %2   freelook: %3", m_fMouseStickPitch.ToString(5, 3), m_fMouseStickRoll.ToString(5, 3), IsMouseFreelookActive().ToString()));
			DbgUI.Text(string.Format("mouse delta: %1 / %2", m_fMouseDbgPitch.ToString(6, 4), m_fMouseDbgRoll.ToString(6, 4)));
		}
		DbgUI.End();
	}

	protected string InputTypeName(EActionValueType t)
	{
		switch (t)
		{
			case EActionValueType.DIGITAL: return "DIGITAL";
			case EActionValueType.ANALOG: return "ANALOG";
			case EActionValueType.ANALOG_RELATIVE: return "ANALOG_REL";
			case EActionValueType.ANALOG_MOTION: return "ANALOG_MOTION";
		}
		return "?";
	}

	protected string AnalogLatchName()
	{
		if (m_bPitchInputAnalog)
			return "analog(spring)";
		return "digital(persistent)";
	}

	void ApplyGroundSteering()
	{
		if (!m_InputMgr || !m_bLocalPilotActive)
			return;

		// m_bLocalPilotActive only refreshes while owner (PollInput); death transfers ownership
		// with the flag stuck true and this would zero CarThrust globally forever
		if (!LocalIsPiloting())
		{
			m_bLocalPilotActive = false;
			return;
		}

		m_InputMgr.ResetAction("CarThrust");
		m_InputMgr.SetActionValue("CarThrust", 0);

		m_InputMgr.ResetAction("CarBrake");
		m_InputMgr.SetActionValue("CarBrake", 0);
		m_InputMgr.ResetAction("CarShift");
		m_InputMgr.SetActionValue("CarShift", 0);

		if (m_fGroundSteerScale == 0)
			return;
		m_InputMgr.ResetAction("CarSteering");
		m_InputMgr.SetActionValue("CarSteering", Math.Clamp(m_fYawInput * m_fGroundSteerScale, -1, 1));
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcSrv_ReceiveInputs(float pitch, float roll, float yaw, float throttle)
	{
		m_fPitchInput = pitch;
		m_fRollInput = roll;
		m_fYawInput = yaw;
		m_fThrottle = throttle;
	}

	// Variant hook (JetFlightCore etc.): per-frame pitch slew rate (units/sec).
	protected float GetPitchSlewRate(float current, float target)
	{
		return m_fPitchControlRate;
	}

	static float MoveTowards(float current, float target, float maxDelta)
	{
		float diff = target - current;
		if (diff > maxDelta) return current + maxDelta;
		if (diff < -maxDelta) return current - maxDelta;
		return target;
	}

	void SetThrottleLocal(float value)
	{
		m_fThrottle = Math.Clamp(value, 0, 1);
	}

	bool IsLocalPilotActive() { return m_bLocalPilotActive; }
	float GetPitchInput() { return m_fPitchInput; }
	float GetRollInput()  { return m_fRollInput;  }
	float GetYawInput()   { return m_fYawInput;   }
	float GetThrottle()   { return m_fThrottle;   }
}
