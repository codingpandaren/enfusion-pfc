[BaseContainerProps()]
class PFC_IsInPlaneCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(notnull SCR_AvailableActionsConditionData data)
	{
		if (!data)
			return false;

		IEntity vehicle = data.GetCurrentVehicle();
		if (!vehicle)
			return GetReturnResult(false);

		bool isPlane = (PFC_FlightModel.Cast(vehicle.FindComponent(PFC_FlightModel)) != null);
		return GetReturnResult(isPlane);
	}
}
