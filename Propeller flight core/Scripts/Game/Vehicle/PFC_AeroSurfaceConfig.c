[BaseContainerProps()]
class PFC_AeroSurfaceConfig
{
	[Attribute("6.28", UIWidgets.EditBox, "Lift curve slope per radian (thin-airfoil max = 2*PI).")]
	float m_fLiftSlope;

	[Attribute("0.02", UIWidgets.EditBox, "Skin-friction drag at zero lift.")]
	float m_fSkinFriction;

	[Attribute("0", UIWidgets.EditBox, "Zero-lift AoA (degrees).")]
	float m_fZeroLiftAoA;

	[Attribute("15", UIWidgets.EditBox, "Positive stall angle (degrees).")]
	float m_fStallAngleHigh;

	[Attribute("-15", UIWidgets.EditBox, "Negative stall angle (degrees).")]
	float m_fStallAngleLow;

	[Attribute("4.0", UIWidgets.EditBox, "Chord length (meters).")]
	float m_fChord;

	[Attribute("10.0", UIWidgets.EditBox, "Span length (meters).")]
	float m_fSpan;

	[Attribute("0", UIWidgets.EditBox, "Flap fraction of chord (0..1). 0 = fixed; >0 = control surface.")]
	float m_fFlapFraction;

	[Attribute("0.85", UIWidgets.EditBox, "Oswald efficiency factor.")]
	float m_fEfficiency;

	// Override when wing is split into multiple panels — per-panel AR produces too much induced drag.
	[Attribute("0", UIWidgets.EditBox, "Override aspect ratio for induced drag. 0 = use span/chord.")]
	float m_fAspectRatioOverride;

	float GetArea()
	{
		return m_fChord * m_fSpan;
	}

	float GetAspectRatio()
	{
		if (m_fAspectRatioOverride > 0)
			return m_fAspectRatioOverride;
		if (m_fChord <= 0.001)
			return 1;
		return m_fSpan / m_fChord;
	}

	bool IsControlSurface()
	{
		return m_fFlapFraction > 0;
	}
}
