enum PFC_ControlAxis
{
	NONE,
	ELEVATOR,
	AILERON,
	RUDDER,
	FLAPS,
}

[BaseContainerProps()]
class PFC_AeroSurfaceDef
{
	[Attribute("Surface", UIWidgets.EditBox, "Label for logs and debug gizmos.")]
	string m_sLabel;

	[Attribute("0 0 0", UIWidgets.EditBox, "Local position (m). Mirror negates X.")]
	vector m_vPosition;

	[Attribute("0 0 0", UIWidgets.EditBox, "Local rotation (yaw pitch roll, degrees).")]
	vector m_vRotation;

	[Attribute("1", UIWidgets.CheckBox, "Spawn X-mirrored counterpart. Ailerons flip deflection sign.")]
	bool m_bMirror;

	[Attribute("0", UIWidgets.ComboBox, "Control axis driving this surface's flap deflection.", "", ParamEnumArray.FromEnum(PFC_ControlAxis))]
	int m_iControlAxis;

	[Attribute("4.5", UIWidgets.EditBox, "Chord length (m).")]
	float m_fChord;

	[Attribute("8.0", UIWidgets.EditBox, "Span length (m).")]
	float m_fSpan;

	[Attribute("5.5", UIWidgets.EditBox, "Lift curve slope per radian.")]
	float m_fLiftSlope;

	[Attribute("0.02", UIWidgets.EditBox, "Skin-friction drag at zero lift.")]
	float m_fSkinFriction;

	[Attribute("0", UIWidgets.EditBox, "Zero-lift AoA (deg).")]
	float m_fZeroLiftAoA;

	[Attribute("15", UIWidgets.EditBox, "Positive stall angle (deg).")]
	float m_fStallAngleHigh;

	[Attribute("-15", UIWidgets.EditBox, "Negative stall angle (deg).")]
	float m_fStallAngleLow;

	[Attribute("0", UIWidgets.EditBox, "Flap fraction of chord (0..1). 0 = fixed; >0 = control surface.")]
	float m_fFlapFraction;

	[Attribute("0.85", UIWidgets.EditBox, "Oswald efficiency factor.")]
	float m_fEfficiency;

	[Attribute("0", UIWidgets.EditBox, "Override AR for induced drag. 0 = use span/chord.")]
	float m_fAspectRatioOverride;

	PFC_AeroSurfaceConfig BuildConfig()
	{
		PFC_AeroSurfaceConfig cfg = new PFC_AeroSurfaceConfig();
		cfg.m_fLiftSlope = m_fLiftSlope;
		cfg.m_fSkinFriction = m_fSkinFriction;
		cfg.m_fZeroLiftAoA = m_fZeroLiftAoA;
		cfg.m_fStallAngleHigh = m_fStallAngleHigh;
		cfg.m_fStallAngleLow = m_fStallAngleLow;
		cfg.m_fChord = m_fChord;
		cfg.m_fSpan = m_fSpan;
		cfg.m_fFlapFraction = m_fFlapFraction;
		cfg.m_fEfficiency = m_fEfficiency;
		cfg.m_fAspectRatioOverride = m_fAspectRatioOverride;
		return cfg;
	}

	void ResolveAxes(out vector outNormal, out vector outForward)
	{
		vector mat[3];
		Math3D.AnglesToMatrix(m_vRotation, mat);
		outNormal = mat[1];
		outForward = mat[2];
	}
}
