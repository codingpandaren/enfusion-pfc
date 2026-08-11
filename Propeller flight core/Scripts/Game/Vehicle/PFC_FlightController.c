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
