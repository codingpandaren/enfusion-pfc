modded class SCR_SettingsSuperMenu
{
	protected bool m_bPFC_FlightControlsTabAdded;

	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		if (m_bPFC_FlightControlsTabAdded || !m_SuperMenuComponent)
			return;

		SCR_TabViewComponent tabView = m_SuperMenuComponent.GetTabView();
		if (!tabView)
			return;

		tabView.AddTab(
			"{69A8A340000C0005}UI/layouts/Menus/SettingsMenu/PFC_FlightControlsSettings.layout",
			"Flight Controls",
			identifier: "PFC_FlightControls"
		);

		m_bPFC_FlightControlsTabAdded = true;
	}
}
