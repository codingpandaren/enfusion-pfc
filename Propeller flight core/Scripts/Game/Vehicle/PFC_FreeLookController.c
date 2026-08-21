[ComponentEditorProps(category: "GameScripted/PFC", description: "Device-aware forced free look for pilot seats: mouse/keyboard always free-looks, gamepad stays locked unless the free-look modifier is held. Local pilot only, per client.")]
class PFC_FreeLookControllerClass : ScriptGameComponentClass
{
}

class PFC_FreeLookController : ScriptGameComponent
{
	protected bool m_bLocalSeated;
	protected bool m_bApplied;
	protected bool m_bAppliedValue;
	protected PFC_FlightController m_Flight;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Flight = PFC_FlightController.Cast(owner.FindComponent(PFC_FlightController));
		EventHandlerManagerComponent ev = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (ev)
		{
			ev.RegisterScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered, false);
			ev.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft, false);
		}
		SetEventMask(owner, EntityEvent.FRAME);
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

	protected void OnCompartmentEntered(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		if (occupant != SCR_PlayerController.GetLocalControlledEntity() || !IsPilotSlot(mgr, slotID, managerId))
			return;
		m_bLocalSeated = true;
		m_bApplied = false;
	}

	protected void OnCompartmentLeft(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		if (occupant != SCR_PlayerController.GetLocalControlledEntity() || !IsPilotSlot(mgr, slotID, managerId))
			return;
		Apply(occupant, false);
		m_bLocalSeated = false;
		m_bApplied = false;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bLocalSeated)
			return;
		IEntity local = SCR_PlayerController.GetLocalControlledEntity();
		if (!local)
			return;
		
		if (!LocalInPilotSeat(local, owner))
		{
			Apply(local, false);
			m_bLocalSeated = false;
			return;
		}
		InputManager im = GetGame().GetInputManager();
		if (!im)
			return;
		ChimeraCharacter ch = ChimeraCharacter.Cast(local);
		if (!ch)
			return;
		CharacterControllerComponent ctrl = ch.GetCharacterController();
		if (!ctrl)
			return;

		bool mouseFlying = false;
		if (m_Flight)
			mouseFlying = m_Flight.IsMouseFlightActive() && !m_Flight.IsMouseFreelookActive();

		bool desired;
		if (im.IsUsingMouseAndKeyboard())
			desired = !mouseFlying;
		else
			desired = !CharacterControllerComponent.GetGamepadControlAircraft();

		if (!m_bApplied || desired != m_bAppliedValue)
		{
			ctrl.SetForcedFreeLook(desired);
			m_bApplied = true;
			m_bAppliedValue = desired;
		}

		if (!desired && im.GetActionValue("Freelook") < 0.1)
			ctrl.SetFreeLook(false, false, false);
	}

	protected void Apply(IEntity character, bool enabled)
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(character);
		if (!ch)
			return;
		CharacterControllerComponent ctrl = ch.GetCharacterController();
		if (ctrl)
			ctrl.SetForcedFreeLook(enabled);
		m_bApplied = true;
		m_bAppliedValue = enabled;
	}

	protected bool IsPilotSlot(BaseCompartmentManagerComponent mgr, int slotID, int managerId)
	{
		if (!mgr)
			return false;
		BaseCompartmentSlot slot = mgr.FindCompartment(slotID, managerId);
		return PilotCompartmentSlot.Cast(slot) != null;
	}

	protected bool LocalInPilotSeat(IEntity local, IEntity vehicle)
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(local);
		if (!ch)
			return false;
		CompartmentAccessComponent access = ch.GetCompartmentAccessComponent();
		if (!access)
			return false;
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || slot.GetOwner().GetRootParent() != vehicle)
			return false;
		return PilotCompartmentSlot.Cast(slot) != null;
	}
}
