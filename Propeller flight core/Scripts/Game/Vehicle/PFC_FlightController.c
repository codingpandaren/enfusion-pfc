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

	[Attribute("-1.0", UIWidgets.EditBox, "Nose-wheel ground steering scale. The smoothed PFC_Yaw (Q/E) input is written into the native CarSteering action so the wheel sim steers the nose wheel, freeing A/D to roll only. Sign matches rudder convention (Q=left, E=right); flip to invert; 0 disables (A/D steers as vanilla).")]
	protected float m_fGroundSteerScale;

	[Attribute("16", UIWidgets.EditBox, "Milliseconds between input RPCs to server.")]
	protected float m_fInputSendIntervalMs;

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
		float pitchMaxDelta = m_fPitchControlRate * timeSlice;
		float yawMaxDelta   = m_fYawControlRate * timeSlice;

		if (!m_bLocalPilotActive || !m_InputMgr)
		{
			m_fPitchInput = MoveTowards(m_fPitchInput, 0, pitchMaxDelta);
			m_fRollInput  = MoveTowards(m_fRollInput,  0, rollMaxDelta);
			m_fYawInput   = MoveTowards(m_fYawInput,   0, yawMaxDelta);
		}
		else
		{
			float pitch = m_InputMgr.GetActionValue("PFC_Pitch");
			float pitchTarget = Math.Clamp(pitch, -1, 1) * m_fPitchInputScale;
			m_fPitchInput = MoveTowards(m_fPitchInput, pitchTarget, pitchMaxDelta);

			float roll = m_InputMgr.GetActionValue("PFC_Roll");
			m_fRollInput = MoveTowards(m_fRollInput, Math.Clamp(roll, -1, 1), rollMaxDelta);

			float yaw = m_InputMgr.GetActionValue("PFC_Yaw");
			m_fYawInput = MoveTowards(m_fYawInput, Math.Clamp(yaw, -1, 1), yawMaxDelta);

			float thrUp   = m_InputMgr.GetActionValue("PFC_ThrottleUp");
			float thrDown = m_InputMgr.GetActionValue("PFC_ThrottleDown");
			if (thrUp > 0.5)
				m_fThrottle = Math.Clamp(m_fThrottle + timeSlice * m_fThrottleRate, 0, 1);
			if (thrDown > 0.5)
				m_fThrottle = Math.Clamp(m_fThrottle - timeSlice * m_fThrottleRate, 0, 1);
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

	void ApplyGroundSteering()
	{
		if (!m_InputMgr || !m_bLocalPilotActive)
			return;

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
