class PFC_PilotUtil
{
	static bool IsLocalActivePilot(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!ch)
			return false;
		CompartmentAccessComponent access = ch.GetCompartmentAccessComponent();
		if (!access)
			return false;
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || slot.GetOwner().GetRootParent() != vehicle)
			return false;
		return slot.IsPiloting();
	}

	static bool IsGamepadFreelookActive()
	{
		InputManager im = GetGame().GetInputManager();
		if (!im || im.IsUsingMouseAndKeyboard())
			return false;
		return im.GetActionValue("Freelook") > 0.1;
	}
}
