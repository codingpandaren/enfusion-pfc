class PFC_AeroSurface
{
	protected ref PFC_AeroSurfaceConfig m_Config;
	protected vector m_vLocalPos;
	protected vector m_vLocalNormal;
	protected vector m_vLocalForward;
	protected float m_fFlapAngleDeg;

	static const float MAX_FLAP_DEG = 45;
	static const float STALL_BLEND_RAD = 15 * Math.DEG2RAD;

	void PFC_AeroSurface(PFC_AeroSurfaceConfig config, vector localPos, vector localNormal, vector localForward)
	{
		m_Config = config;
		m_vLocalPos = localPos;
		m_vLocalNormal = localNormal.Normalized();
		m_vLocalForward = localForward.Normalized();
		m_fFlapAngleDeg = 0;
	}

	void SetFlapAngle(float deg)
	{
		m_fFlapAngleDeg = Math.Clamp(deg, -MAX_FLAP_DEG, MAX_FLAP_DEG);
	}

	float GetFlapAngle() { return m_fFlapAngleDeg; }
	vector GetLocalPosition() { return m_vLocalPos; }
	vector GetLocalNormal() { return m_vLocalNormal; }
	vector GetLocalForward() { return m_vLocalForward; }
	PFC_AeroSurfaceConfig GetConfig() { return m_Config; }

	void CalculateForce(vector airflowLocal, float airDensity, out vector outForceLocal)
	{
		outForceLocal = vector.Zero;

		float speedSq = vector.Dot(airflowLocal, airflowLocal);
		if (speedSq < 0.01)
			return;

		float speed = Math.Sqrt(speedSq);
		vector flowDir = airflowLocal * (1.0 / speed);
		vector dragDir = -flowDir;

		float normalDotFlow = vector.Dot(flowDir, m_vLocalNormal);
		vector liftDir = m_vLocalNormal - flowDir * normalDotFlow;
		float liftDirLen = liftDir.Length();
		if (liftDirLen < 0.0001)
			return;
		liftDir = liftDir * (1.0 / liftDirLen);

		float aoaRad = Math.Asin(Math.Clamp(-normalDotFlow, -1, 1));

		float effectiveAoARad = aoaRad;
		if (m_Config.IsControlSurface())
			effectiveAoARad += m_fFlapAngleDeg * Math.DEG2RAD * m_Config.m_fFlapFraction;

		float cl = CalculateLiftCoefficient(effectiveAoARad);
		float cd = CalculateDragCoefficient(effectiveAoARad, cl);

		float area = m_Config.GetArea();
		float dynamicPressure = 0.5 * airDensity * speedSq;

		vector liftForce = liftDir * (cl * dynamicPressure * area);
		vector dragForce = dragDir * (cd * dynamicPressure * area);

		outForceLocal = liftForce + dragForce;
	}

	protected float CalculateLiftCoefficient(float aoaRad)
	{
		float zeroLiftRad = m_Config.m_fZeroLiftAoA * Math.DEG2RAD;
		float stallHighRad = m_Config.m_fStallAngleHigh * Math.DEG2RAD;
		float stallLowRad  = m_Config.m_fStallAngleLow  * Math.DEG2RAD;

		if (aoaRad >= stallLowRad && aoaRad <= stallHighRad)
			return m_Config.m_fLiftSlope * (aoaRad - zeroLiftRad);

		float stallAngleRad;
		float clAtStall;
		if (aoaRad > stallHighRad)
		{
			stallAngleRad = stallHighRad;
			clAtStall = m_Config.m_fLiftSlope * (stallHighRad - zeroLiftRad);
		}
		else
		{
			stallAngleRad = stallLowRad;
			clAtStall = m_Config.m_fLiftSlope * (stallLowRad - zeroLiftRad);
		}

		float flatPlateCl = 2 * Math.Sin(aoaRad) * Math.Cos(aoaRad);
		float delta = Math.AbsFloat(aoaRad - stallAngleRad);
		if (delta >= STALL_BLEND_RAD)
			return flatPlateCl;

		float t = delta / STALL_BLEND_RAD;
		float blend = 0.5 + 0.5 * Math.Cos(Math.PI * t);
		return clAtStall * blend + flatPlateCl * (1.0 - blend);
	}

	protected float CalculateDragCoefficient(float aoaRad, float cl)
	{
		float skin = m_Config.m_fSkinFriction;

		float ar = m_Config.GetAspectRatio();
		float e = m_Config.m_fEfficiency;
		float induced = 0;
		if (ar > 0 && e > 0)
			induced = (cl * cl) / (Math.PI * ar * e);

		float sinAoA = Math.Sin(aoaRad);
		float postStall = sinAoA * sinAoA * 0.5;

		return skin + induced + postStall;
	}
}
