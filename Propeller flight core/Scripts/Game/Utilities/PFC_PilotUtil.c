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
		if (!CharacterControllerComponent.GetGamepadControlAircraft())
			return true;
		return im.GetActionValue("Freelook") > 0.1;
	}

	protected static ref map<string, bool> s_LookConflictCache;
	protected static ref array<string> s_aLookAxes;
	protected static float s_fLookCacheRefreshMs = -1e9;

	static bool IsFlightInputSuppressed(string actionName)
	{
		if (!IsGamepadFreelookActive())
			return false;
		return IsActionOnLookStick(actionName);
	}

	static bool IsActionOnLookStick(string actionName)
	{
		float now = System.GetTickCount();
		if (!s_LookConflictCache || now - s_fLookCacheRefreshMs > 3000)
		{
			s_LookConflictCache = new map<string, bool>();
			s_aLookAxes = null;
			s_fLookCacheRefreshMs = now;
		}

		bool cached;
		if (s_LookConflictCache.Find(actionName, cached))
			return cached;

		InputBinding binding = GetGame().GetInputManager().CreateUserBinding();
		if (!binding)
			return true;

		if (!s_aLookAxes)
		{
			s_aLookAxes = {};
			array<string> lookBinds = {};
			binding.GetBindings("MouseX", lookBinds, EInputDeviceType.GAMEPAD);
			binding.GetBindings("MouseY", lookBinds, EInputDeviceType.GAMEPAD);
			foreach (string lb : lookBinds)
			{
				string axis = StripAxisDirection(lb);
				if (!axis.IsEmpty() && s_aLookAxes.Find(axis) < 0)
					s_aLookAxes.Insert(axis);
			}
			if (s_aLookAxes.IsEmpty())
			{
				s_aLookAxes.Insert("gamepad0:right_thumb_horizontal");
				s_aLookAxes.Insert("gamepad0:right_thumb_vertical");
			}
		}

		bool conflict = false;
		array<string> binds = {};
		binding.GetBindings(actionName, binds, EInputDeviceType.GAMEPAD);
		foreach (string b : binds)
		{
			if (s_aLookAxes.Find(StripAxisDirection(b)) >= 0)
			{
				conflict = true;
				break;
			}
		}
		s_LookConflictCache.Insert(actionName, conflict);
		return conflict;
	}

	protected static string StripAxisDirection(string bind)
	{
		int len = bind.Length();
		if (len == 0)
			return bind;
		string last = bind.Substring(len - 1, 1);
		if (last == "+" || last == "-")
			return bind.Substring(0, len - 1);
		return bind;
	}
}
