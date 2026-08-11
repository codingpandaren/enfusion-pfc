modded enum SCR_DebugMenuID
{
	PFC_ROOT,
	PFC_DEBUG_DRAW,
	PFC_DEBUG_APPROACH,
	PFC_DEBUG_INPUT,
}

[ComponentEditorProps(category: "GameScripted/PropFlightCore", description: "Flight model. Per-surface lift/drag + thrust via rigid body impulses. Runs on owner + server; proxies receive state via PFC_NwkMovementComponent.")]
class PFC_FlightModelClass : ScriptGameComponentClass
{
}

class PFC_FlightModel : ScriptGameComponent
{
	[Attribute("2500", UIWidgets.EditBox, "Max thrust per engine (N).")]
	protected float m_fMaxThrustPerEngine;

	[Attribute("0.25", UIWidgets.Slider, "Fraction of thrust still available at zero hull health. Thrust scales linearly with the vehicle's main health between full (1.0) and this floor. 1 = damage has no effect on power.", params: "0 1 0.01")]
	protected float m_fMinHealthThrustFraction;

	[Attribute("1", UIWidgets.EditBox, "Number of engines.")]
	protected int m_iNumEngines;

	[Attribute("25", UIWidgets.EditBox, "Max control surface deflection (deg).")]
	protected float m_fMaxControlDeflectionDeg;

	[Attribute("40", UIWidgets.EditBox, "Flap deflection when deployed (deg). The minimal core has no flap controller, so flaps stay retracted unless a variant drives a FLAPS surface.")]
	protected float m_fFlapDeflectionDeg;

	[Attribute("2.0", UIWidgets.EditBox, "Flap deploy/retract duration (seconds).")]
	protected float m_fFlapDeployDurationSeconds;

	[Attribute("1.225", UIWidgets.EditBox, "Air density at sea level (kg/m^3).")]
	protected float m_fAirDensitySeaLevel;

	[Attribute("0.0001", UIWidgets.EditBox, "Linear density falloff per metre of altitude.")]
	protected float m_fDensityFalloff;

	[Attribute("1.0", UIWidgets.EditBox, "Wind effect scale. Multiplies the world (weather) wind before it's applied as relative airflow: 0 = ignore wind, 1 = real m/s, >1 = stronger for gameplay. Aero forces use airspeed (velocity relative to the air), so this drives crosswind drift, weathervaning, and the IAS/groundspeed split.")]
	protected float m_fWindScale;

	[Attribute("0.5", UIWidgets.Slider, "Near-ground wind gradient: wind fraction at the surface (AGL 0). Wind eases from this fraction up to full at the gradient height, modelling the boundary layer so the crosswind shears as you descend. 1 = no gradient.", params: "0 1 0.01")]
	protected float m_fWindSurfaceFraction;

	[Attribute("150", UIWidgets.EditBox, "Near-ground wind gradient: AGL (m) at which the wind reaches full strength.")]
	protected float m_fWindGradientHeight;

	[Attribute("0.3", UIWidgets.EditBox, "Gust intensity as a fraction of the steady wind speed (0 = smooth, steady wind only). Adds smooth time-varying turbulence to wind speed and direction for buffeting. Scales with wind, so calm air stays calm.")]
	protected float m_fGustIntensity;

	[Attribute("0.5", UIWidgets.Slider, "Vertical gust strength relative to horizontal gusts. Vertical bumps (updraft/downdraft) are the most felt; 0 = horizontal gusts only.", params: "0 2 0.05")]
	protected float m_fGustVerticalScale;

	[Attribute("0.6", UIWidgets.EditBox, "Fuselage longitudinal drag (CD * A, m^2).")]
	protected float m_fFuselageDragArea;

	[Attribute("2.0", UIWidgets.EditBox, "Angular velocity decay rate (1/sec).")]
	protected float m_fAngularDamping;

	[Attribute("0", UIWidgets.CheckBox, "Initial debug draw toggle state. Runtime: F6 -> Prop Flight -> Debug draw.")]
	protected bool m_bDebugDraw;

	protected static bool s_bDiagRegistered;

	[Attribute("800", UIWidgets.EditBox, "Engine idle RPM.")]
	protected float m_fIdleRPM;

	[Attribute("2700", UIWidgets.EditBox, "Engine max RPM.")]
	protected float m_fMaxRPM;

	[Attribute("0.5", UIWidgets.EditBox, "RPM ramp rate (fraction of max RPM per second).")]
	protected float m_fRPMRate;

	[Attribute("0.5", UIWidgets.EditBox, "Radar altimeter smoothing tau (seconds). Cuts treetop/vehicle spike noise.")]
	protected float m_fAltAGLSmoothingTau;

	protected float m_fAltAGLSmoothed;

	protected ref LocalWeatherSituation m_WeatherSituation;
	protected vector m_vWindWS;

	vector GetWindWS() { return m_vWindWS; }
	protected vector m_vSteadyWindWS;
	protected float m_fSteadyWindSpeed;
	protected float m_fWindRefreshTimer;
	protected float m_fGustTime;
	protected const float WIND_REFRESH_INTERVAL = 0.5;

	[Attribute("", UIWidgets.Object, "Aerodynamic surface definitions.")]
	protected ref array<ref PFC_AeroSurfaceDef> m_aSurfaceDefs;

	protected ref array<ref PFC_AeroSurface> m_aSurfaces = {};
	protected ref array<int> m_aSurfaceAxes = {};
	protected ref array<bool> m_aSurfaceFlipSign = {};

	protected ref array<vector> m_aDebugForces = {};
	protected ref array<vector> m_aDebugPositions = {};

	protected PFC_FlightController m_FlightController;
	protected RplComponent m_RplComponent;
	protected SignalsManagerComponent m_SignalsMgr;
	protected SCR_DamageManagerComponent m_DamageManager;

	protected int m_iSigAirSpeed   = -1;
	protected int m_iSigAltitude   = -1;
	protected int m_iSigAltAGL     = -1;
	protected int m_iSigClimbRate  = -1;
	protected int m_iSigRPM        = -1;
	protected int m_iSigRotorRPMScaled = -1;

	protected ref array<int> m_aSigPropAngles = {};
	protected ref array<int> m_aSigEngineRPMs = {};
	protected int m_iSigElevator   = -1;
	protected int m_iSigRudder     = -1;
	protected int m_iSigAileron    = -1;
	protected int m_iSigFlap       = -1;
	protected int m_iSigPitch      = -1;
	protected int m_iSigBank       = -1;
	protected int m_iSigHeading    = -1;
	protected int m_iSigGroundSpeed = -1;
	protected int m_iSigGLoad       = -1;

	protected float m_fCurrentFlapAngle;

	protected ref array<float> m_aPropAngles = {};
	protected ref array<float> m_aEngineRPMs = {};
	protected float m_fEngineRPM;
	protected float m_fDebugAoA;
	protected float m_fDbgAoA;
	protected float m_fDbgPitch;
	protected float m_fDbgFPA;
	protected float m_fDbgIAS;
	protected float m_fDbgVS;
	protected vector m_vPrevVelocity;
	protected float m_fDbgGNormal;
	protected float m_fDbgGLateral;
	protected float m_fDbgGLongitudinal;

	protected float m_fGLoad = 1.0;
	protected vector m_vPrevVelSim;
	protected bool m_bPrevVelSimValid;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.SIMULATE | EntityEvent.INIT | EntityEvent.FRAME);
		owner.SetFlags(EntityFlags.ACTIVE, true);
		BuildSurfaces();
		RegisterDiagMenu();
		if (m_bDebugDraw)
			DiagMenu.SetValue(SCR_DebugMenuID.PFC_DEBUG_DRAW, 1);
	}

	protected static void RegisterDiagMenu()
	{
		if (s_bDiagRegistered)
			return;
		s_bDiagRegistered = true;
		DiagMenu.RegisterMenu(SCR_DebugMenuID.PFC_ROOT, "Prop Flight", "");
		DiagMenu.RegisterBool(SCR_DebugMenuID.PFC_DEBUG_DRAW, "f6", "Debug draw", "Prop Flight");
		DiagMenu.RegisterBool(SCR_DebugMenuID.PFC_DEBUG_APPROACH, "", "Setup approach", "Prop Flight");
		DiagMenu.RegisterBool(SCR_DebugMenuID.PFC_DEBUG_INPUT, "", "Input debug (raw vs smoothed)", "Prop Flight");
	}

	override void EOnInit(IEntity owner)
	{
		m_FlightController = PFC_FlightController.Cast(owner.FindComponent(PFC_FlightController));
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_SignalsMgr = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
		m_DamageManager = SCR_DamageManagerComponent.Cast(owner.FindComponent(SCR_DamageManagerComponent));

		if (m_SignalsMgr)
		{
			m_iSigAirSpeed  = m_SignalsMgr.AddOrFindMPSignal("airspeed",    0.1, 30, 0, SignalCompressionFunc.None);
			m_iSigAltitude  = m_SignalsMgr.AddOrFindMPSignal("altitude",    0.5, 30, 0, SignalCompressionFunc.None);
			m_iSigAltAGL    = m_SignalsMgr.AddOrFindMPSignal("altitudeAGL", 0.5, 30, 0, SignalCompressionFunc.None);
			m_iSigClimbRate = m_SignalsMgr.AddOrFindMPSignal("climbRate",   0.1, 30, 0, SignalCompressionFunc.None);
			m_iSigRPM       = m_SignalsMgr.AddOrFindMPSignal("RPM",        0.1, 30, 0, SignalCompressionFunc.Range01);
			m_iSigRotorRPMScaled = m_SignalsMgr.AddOrFindMPSignal("MainRotorRPMScaled", 0.1, 30, 0, SignalCompressionFunc.Range01);
			m_aSigPropAngles.Clear();
			m_aSigEngineRPMs.Clear();
			m_aPropAngles.Clear();
			m_aEngineRPMs.Clear();
			for (int eng = 0; eng < m_iNumEngines; eng++)
			{
				int angleSigId = m_SignalsMgr.AddOrFindSignal("PropellerAngle_" + (eng + 1).ToString());
				m_aSigPropAngles.Insert(angleSigId);
				int rpmSigId = m_SignalsMgr.AddOrFindMPSignal("EngineRPM_" + (eng + 1).ToString(), 0.02, 10, 0, SignalCompressionFunc.Range01);
				m_aSigEngineRPMs.Insert(rpmSigId);
				m_aPropAngles.Insert(0);
				m_aEngineRPMs.Insert(0);
			}
			m_iSigElevator  = m_SignalsMgr.AddOrFindMPSignal("ElevatorAngle",  0.05, 30, 0, SignalCompressionFunc.RotDEG);
			m_iSigRudder    = m_SignalsMgr.AddOrFindMPSignal("RudderAngle",    0.05, 30, 0, SignalCompressionFunc.RotDEG);
			m_iSigAileron   = m_SignalsMgr.AddOrFindMPSignal("AileronAngle",   0.05, 30, 0, SignalCompressionFunc.RotDEG);
			m_iSigFlap      = m_SignalsMgr.AddOrFindMPSignal("FlapAngle",      0.05, 30, 0, SignalCompressionFunc.RotDEG);
			m_iSigPitch     = m_SignalsMgr.AddOrFindMPSignal("pitch",          0.5,  30, 0, SignalCompressionFunc.None);
			m_iSigBank      = m_SignalsMgr.AddOrFindMPSignal("bank",           0.5,  30, 0, SignalCompressionFunc.None);
			m_iSigHeading   = m_SignalsMgr.AddOrFindMPSignal("heading",        0.5,  30, 0, SignalCompressionFunc.None);
			m_iSigGroundSpeed = m_SignalsMgr.AddOrFindMPSignal("groundspeed",  0.1,  30, 0, SignalCompressionFunc.None);
			m_iSigGLoad       = m_SignalsMgr.AddOrFindMPSignal("gLoad",        0.02, 30, 0, SignalCompressionFunc.None);
		}

		if (!m_FlightController)
			Print("[PFC_FlightModel] No PFC_FlightController found on entity!", LogLevel.ERROR);
	}

	protected void BuildSurfaces()
	{
		m_aSurfaces.Clear();
		m_aSurfaceAxes.Clear();
		m_aSurfaceFlipSign.Clear();

		if (!m_aSurfaceDefs || m_aSurfaceDefs.IsEmpty())
		{
			Print("[PFC_FlightModel] m_aSurfaceDefs is empty — plane has no aero surfaces!", LogLevel.WARNING);
			return;
		}

		foreach (PFC_AeroSurfaceDef def : m_aSurfaceDefs)
		{
			if (!def)
				continue;
			SpawnSurfaceFromDef(def, false);
			if (def.m_bMirror)
				SpawnSurfaceFromDef(def, true);
		}

		m_aDebugForces.Resize(m_aSurfaces.Count());
		m_aDebugPositions.Resize(m_aSurfaces.Count());
	}

	protected void SpawnSurfaceFromDef(PFC_AeroSurfaceDef def, bool mirrored)
	{
		vector pos = def.m_vPosition;
		vector normal, forward;
		def.ResolveAxes(normal, forward);

		bool flipSign = false;
		if (mirrored)
		{
			pos[0]     = -pos[0];
			normal[0]  = -normal[0];
			forward[0] = -forward[0];
			if (def.m_iControlAxis == PFC_ControlAxis.AILERON)
				flipSign = true;
		}

		PFC_AeroSurface s = new PFC_AeroSurface(def.BuildConfig(), pos, normal, forward);
		m_aSurfaces.Insert(s);
		m_aSurfaceAxes.Insert(def.m_iControlAxis);
		m_aSurfaceFlipSign.Insert(flipSign);
	}

	override void EOnSimulate(IEntity owner, float timeSlice)
	{
		Physics physics = owner.GetPhysics();
		if (!physics || !physics.IsDynamic())
			return;

		if (m_RplComponent && !m_RplComponent.IsOwner() && !Replication.IsServer())
			return;

		float pitch = 0, roll = 0, yaw = 0, throttle = 0;
		if (m_FlightController)
		{
			m_FlightController.PollInput(timeSlice);
			pitch    = m_FlightController.GetPitchInput();
			roll     = m_FlightController.GetRollInput();
			yaw      = m_FlightController.GetYawInput();
			throttle = m_FlightController.GetThrottle();
		}

		float maxDef = m_fMaxControlDeflectionDeg;

		float flapTarget = GetFlapTargetAngle();
		if (m_fFlapDeployDurationSeconds > 0 && m_fFlapDeflectionDeg > 0)
		{
			float flapRate = m_fFlapDeflectionDeg / m_fFlapDeployDurationSeconds;
			if (m_fCurrentFlapAngle < flapTarget)
				m_fCurrentFlapAngle = Math.Min(m_fCurrentFlapAngle + flapRate * timeSlice, flapTarget);
			else
				m_fCurrentFlapAngle = Math.Max(m_fCurrentFlapAngle - flapRate * timeSlice, flapTarget);
		}
		else
		{
			m_fCurrentFlapAngle = flapTarget;
		}

		for (int i = 0; i < m_aSurfaces.Count(); i++)
		{
			float deflection;
			if (!GetSurfaceDeflection(m_aSurfaceAxes[i], pitch, roll, yaw, maxDef, deflection))
				continue;
			if (m_aSurfaceFlipSign[i])
				deflection = -deflection;
			m_aSurfaces[i].SetFlapAngle(deflection);
		}

		vector velocityWS = physics.GetVelocity();

		if (BadVec(velocityWS))
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			return;
		}

		const float MAX_BODY_SPEED = 600.0;
		const float MAX_BODY_ANGSPEED = 40.0;
		float bodySpeed = velocityWS.Length();
		if (bodySpeed > MAX_BODY_SPEED)
		{
			velocityWS = velocityWS * (MAX_BODY_SPEED / bodySpeed);
			physics.SetVelocity(velocityWS);
		}
		vector bodyAngVel = physics.GetAngularVelocity();
		float bodyAngSpeed = bodyAngVel.Length();
		if (IsFiniteVec(bodyAngVel) && bodyAngSpeed > MAX_BODY_ANGSPEED)
			physics.SetAngularVelocity(bodyAngVel * (MAX_BODY_ANGSPEED / bodyAngSpeed));

		const float MAX_AIRSPEED = 400.0;

		RefreshWind(owner, timeSlice);
		vector airVelWS = velocityWS - m_vWindWS;
		float rawSpeed = airVelWS.Length();
		if (rawSpeed > MAX_AIRSPEED)
			airVelWS = airVelWS * (MAX_AIRSPEED / rawSpeed);
		vector velocityLS = owner.VectorToLocal(airVelWS);
		float speed = airVelWS.Length();
		float altitude = owner.GetOrigin()[1];
		float airDensity = GetAirDensity(altitude);

		float aoaDeg = 0;
		if (speed > 1)
		{
			float velLocalZ = velocityLS[2];
			float velLocalY = velocityLS[1];
			aoaDeg = Math.Atan2(-velLocalY, velLocalZ) * Math.RAD2DEG;
		}
		m_fDebugAoA = aoaDeg;

		if (airVelWS.LengthSq() > 1.0)
		{
			for (int i = 0; i < m_aSurfaces.Count(); i++)
			{
				PFC_AeroSurface surf = m_aSurfaces[i];

				vector surfVelLS = GetSurfaceAirVelocityLS(owner, physics, surf.GetLocalPosition(), velocityLS);
				vector forceLS;
				surf.CalculateForce(surfVelLS, airDensity, forceLS);

				vector forceWS = owner.VectorToParent(forceLS);
				vector surfPosWS = owner.CoordToParent(surf.GetLocalPosition());
				physics.ApplyImpulseAt(surfPosWS, forceWS * timeSlice);

				m_aDebugForces[i] = forceWS;
				m_aDebugPositions[i] = surfPosWS;
			}

			vector dragWS = airVelWS * (-0.5 * airDensity * speed * GetFuselageDragArea(speed, airDensity));
			physics.ApplyImpulse(dragWS * timeSlice);
		}
		else
		{
			for (int i = 0; i < m_aSurfaces.Count(); i++)
			{
				m_aDebugForces[i] = vector.Zero;
				m_aDebugPositions[i] = owner.CoordToParent(m_aSurfaces[i].GetLocalPosition());
			}
		}

		bool destroyed = (m_DamageManager && m_DamageManager.GetState() == EDamageState.DESTROYED);

		UpdateEngineSpool(throttle, timeSlice, destroyed);

		float thrustMag = ComputeThrustMagnitude(speed, airDensity, destroyed);
		if (thrustMag > 0)
		{
			vector thrustWS = owner.VectorToParent(Vector(0, 0, thrustMag));
			vector comWS = owner.CoordToParent(physics.GetCenterOfMass());
			physics.ApplyImpulseAt(comWS, thrustWS * timeSlice);
		}

		if (m_fAngularDamping > 0)
		{
			vector angVel = physics.GetAngularVelocity();
			if (IsFiniteVec(angVel))
			{
				float refSpeed = 80;
				float speedFactor = Math.Clamp(speed / refSpeed, 0.1, 1.5);
				float decayPerTick = Math.Min(m_fAngularDamping * speedFactor * timeSlice, 1.0);
				physics.SetAngularVelocity(angVel * (1.0 - decayPerTick));
			}
		}

		if (timeSlice > 0.0001)
		{
			if (m_bPrevVelSimValid)
			{
				vector accelSim = (velocityWS - m_vPrevVelSim) * (1.0 / timeSlice);
				vector gForceSim = accelSim + Vector(0, 9.81, 0);
				vector upSim = owner.VectorToParent(Vector(0, 1, 0));
				float gN = vector.Dot(gForceSim, upSim) / 9.81;
				if (gN == gN)
					m_fGLoad = m_fGLoad + (gN - m_fGLoad) * 0.1;
			}
			m_vPrevVelSim = velocityWS;
			m_bPrevVelSimValid = true;
		}

		OnAeroSimulate(owner, physics, timeSlice, speed, aoaDeg, airDensity);

		bool isOwner = !m_RplComponent || m_RplComponent.IsOwner();
		bool replicationReady = !m_RplComponent || m_RplComponent.Id().IsValid();
		if (isOwner && replicationReady)
		{
			float rawAGL = ComputeAltitudeAGL(owner);
			float alpha = 1.0;
			if (m_fAltAGLSmoothingTau > 0)
				alpha = Math.Clamp(timeSlice / m_fAltAGLSmoothingTau, 0, 1);
			m_fAltAGLSmoothed = m_fAltAGLSmoothed + (rawAGL - m_fAltAGLSmoothed) * alpha;
			float groundHoriz = Math.Sqrt(velocityWS[0] * velocityWS[0] + velocityWS[2] * velocityWS[2]);
			UpdateSignals(owner, speed, altitude, m_fAltAGLSmoothed, velocityWS[1], groundHoriz, throttle, pitch, roll, yaw);
		}
	}

	// Variant hooks (JetFlightCore etc.). Base implementations reproduce the classic prop behaviour exactly.
	protected bool GetSurfaceDeflection(int axis, float pitch, float roll, float yaw, float maxDef, out float deflection)
	{
		deflection = 0;
		switch (axis)
		{
			case PFC_ControlAxis.ELEVATOR: deflection = -Math.Clamp(pitch, -1, 1) * maxDef; return true;
			case PFC_ControlAxis.AILERON:  deflection = -roll * maxDef; return true;
			case PFC_ControlAxis.RUDDER:   deflection = -yaw * maxDef; return true;
			case PFC_ControlAxis.FLAPS:    deflection = m_fCurrentFlapAngle; return true;
		}
		return false;
	}

	protected float GetFuselageDragArea(float speed, float airDensity)
	{
		return m_fFuselageDragArea;
	}

	protected vector GetSurfaceAirVelocityLS(IEntity owner, Physics physics, vector surfLocalPos, vector velocityLS)
	{
		return velocityLS;
	}

	protected float GetFlapTargetAngle()
	{
		return 0;
	}

	protected void UpdateEngineSpool(float throttle, float timeSlice, bool destroyed)
	{
		float rpmDelta = m_fRPMRate * m_fMaxRPM * timeSlice;
		float aggregate = 0;

		for (int i = 0; i < m_aEngineRPMs.Count(); i++)
		{
			float perEngineTarget = 0;
			if (!destroyed)
				perEngineTarget = m_fIdleRPM + throttle * (m_fMaxRPM - m_fIdleRPM);

			float current = m_aEngineRPMs[i];
			if (destroyed)
			{
				current = 0;
			}
			else
			{
				if (current < perEngineTarget)
					current = Math.Min(current + rpmDelta, perEngineTarget);
				else
					current = Math.Max(current - rpmDelta, perEngineTarget);
			}
			m_aEngineRPMs[i] = current;
			aggregate += current;
		}

		if (m_iNumEngines > 0)
			m_fEngineRPM = aggregate / m_iNumEngines;
		else
			m_fEngineRPM = 0;
	}

	protected float GetThrustHealthMultiplier()
	{
		if (!m_DamageManager)
			return 1;
		HitZone defaultHZ = m_DamageManager.GetDefaultHitZone();
		if (!defaultHZ)
			return 1;
		return Math.Lerp(m_fMinHealthThrustFraction, 1, defaultHZ.GetHealthScaled());
	}

	protected float ComputeThrustMagnitude(float speed, float airDensity, bool destroyed)
	{
		if (destroyed)
			return 0;
		float thrustFraction = 0;
		float rpmSpan = m_fMaxRPM - m_fIdleRPM;
		if (rpmSpan > 0)
			thrustFraction = Math.Clamp((m_fEngineRPM - m_fIdleRPM) / rpmSpan, 0, 1);
		if (thrustFraction <= 0.001)
			return 0;
		return thrustFraction * m_fMaxThrustPerEngine * m_iNumEngines * GetThrustHealthMultiplier();
	}

	protected void OnAeroSimulate(IEntity owner, Physics physics, float timeSlice, float speed, float aoaDeg, float airDensity)
	{
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_FlightController)
			m_FlightController.ApplyGroundSteering();

		UpdatePropellerVisuals(timeSlice);
		DrawDebug(owner, timeSlice);

		if (DiagMenu.GetBool(SCR_DebugMenuID.PFC_DEBUG_APPROACH))
		{
			DiagMenu.SetValue(SCR_DebugMenuID.PFC_DEBUG_APPROACH, 0);
			SetupApproach(owner);
		}
	}

	protected void SetupApproach(IEntity owner)
	{
		Physics physics = owner.GetPhysics();
		if (!physics)
			return;

		vector pos = owner.GetOrigin();
		pos[1] = pos[1] + 1000;

		vector mat[4];
		owner.GetWorldTransform(mat);

		float bankDeg = 30;
		float a = bankDeg * Math.DEG2RAD;
		float c = Math.Cos(a);
		float s = Math.Sin(a);
		vector right = mat[0];
		vector up    = mat[1];
		mat[0] = right * c + up * s;
		mat[1] = up * c - right * s;

		mat[3] = pos;
		owner.SetWorldTransform(mat);

		vector fwd = mat[2];
		float approachSpeed = 50;
		physics.SetVelocity(fwd * approachSpeed);
		physics.SetAngularVelocity(vector.Zero);

		if (m_FlightController)
			m_FlightController.SetThrottleLocal(1.0);
	}

	override event bool OnTicksOnRemoteProxy()
	{
		return true;
	}

	protected void UpdatePropellerVisuals(float timeSlice)
	{
		if (!m_SignalsMgr || m_aPropAngles.IsEmpty())
			return;

		bool isOwner = !m_RplComponent || m_RplComponent.IsOwner();
		bool isServer = Replication.IsServer();
		bool isProxy = !isOwner && !isServer;

		for (int i = 0; i < m_aPropAngles.Count(); i++)
		{
			float rpm;
			if (isProxy)
			{
				int rpmSigId = -1;
				if (i < m_aSigEngineRPMs.Count())
					rpmSigId = m_aSigEngineRPMs[i];
				float normalized = 0;
				if (rpmSigId != -1)
					normalized = m_SignalsMgr.GetSignalValue(rpmSigId);
				rpm = normalized * m_fMaxRPM;
			}
			else
			{
				rpm = m_aEngineRPMs[i];
			}

			float deg = m_aPropAngles[i] + rpm * 6.0 * timeSlice;
			while (deg >= 360.0)
				deg -= 360.0;
			m_aPropAngles[i] = deg;

			int propSigId = m_aSigPropAngles[i];
			if (propSigId != -1)
				m_SignalsMgr.SetSignalValue(propSigId, deg);
		}
	}

	protected override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		DrawDebug(owner, timeSlice);
	}

	protected void DrawDebug(IEntity owner, float timeSlice)
	{
		if (!DiagMenu.GetBool(SCR_DebugMenuID.PFC_DEBUG_DRAW))
			return;

		vector origin = owner.GetOrigin();
		BaseWorld world = GetGame().GetWorld();
		const int TEXT_SIZE = 7;
		const int TEXT_COLOR = ARGB(255, 80, 255, 80);
		const int TEXT_BG = ARGB(180, 0, 0, 0);
		const int TEXT_FLAGS = DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA;

		vector fwd   = owner.VectorToParent(Vector(0, 0, 1));
		vector right = owner.VectorToParent(Vector(1, 0, 0));
		vector up    = owner.VectorToParent(Vector(0, 1, 0));
		Shape.CreateArrow(origin, origin + fwd * 3,   0.15, Color.RED,   ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
		Shape.CreateArrow(origin, origin + right * 3, 0.15, Color.GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
		Shape.CreateArrow(origin, origin + up * 3,    0.15, Color.BLUE,  ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);

		if (m_vWindWS.LengthSq() > 0.01)
			Shape.CreateArrow(origin, origin + m_vWindWS, 0.2, ARGB(255, 120, 200, 255), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);

		float sm = 0.15;
		Physics physics = owner.GetPhysics();
		if (physics)
		{
			vector vel = physics.GetVelocity();
			float speed = vel.Length();
			if (speed > 0.1)
			{
				vector flightDir = vel * (1.0 / speed);
				vector flightTip = origin + flightDir * 15;
				Shape.CreateArrow(origin, flightTip, 0.25, Color.ORANGE, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);

				float flightPathDeg = Math.Asin(Math.Clamp(flightDir[1], -1, 1)) * Math.RAD2DEG;
				float pitchDeg = Math.Asin(Math.Clamp(fwd[1], -1, 1)) * Math.RAD2DEG;

				m_fDbgAoA   = m_fDbgAoA   + (m_fDebugAoA - m_fDbgAoA) * sm;
				m_fDbgPitch = m_fDbgPitch + (pitchDeg - m_fDbgPitch) * sm;
				m_fDbgFPA   = m_fDbgFPA   + (flightPathDeg - m_fDbgFPA) * sm;
				m_fDbgIAS   = m_fDbgIAS   + (speed * 3.6 - m_fDbgIAS) * sm;
				m_fDbgVS    = m_fDbgVS    + (vel[1] - m_fDbgVS) * sm;
			}

			if (timeSlice > 0.0001)
			{
				vector accel = (vel - m_vPrevVelocity) * (1.0 / timeSlice);
				vector gForceWS = accel + Vector(0, 9.81, 0);

				float gNormal = vector.Dot(gForceWS, up) / 9.81;
				float gLateral = vector.Dot(gForceWS, right) / 9.81;
				float gLongitudinal = vector.Dot(gForceWS, fwd) / 9.81;

				m_fDbgGNormal = m_fDbgGNormal + (gNormal - m_fDbgGNormal) * sm;
				m_fDbgGLateral = m_fDbgGLateral + (gLateral - m_fDbgGLateral) * sm;
				m_fDbgGLongitudinal = m_fDbgGLongitudinal + (gLongitudinal - m_fDbgGLongitudinal) * sm;

				float gArrowScale = 3.0;
				vector gVecScaled = gForceWS * (gArrowScale / 9.81);
				int gColor = ARGB(255, 0, 255, 255);
				if (gVecScaled.LengthSq() > 0.01)
					Shape.CreateArrow(origin, origin + gVecScaled, 0.2, gColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			}
			m_vPrevVelocity = vel;
		}

		if (DiagMenu.GetBool(SCR_DebugMenuID.PFC_DEBUG_DRAW))
		{
			DbgUI.Begin("Prop Flight Data");
			DbgUI.Text(string.Format("AoA:   %1°", Math.Round(m_fDbgAoA * 10) * 0.1));
			DbgUI.Text(string.Format("Pitch: %1°", Math.Round(m_fDbgPitch * 10) * 0.1));
			DbgUI.Text(string.Format("FPA:   %1°", Math.Round(m_fDbgFPA * 10) * 0.1));
			DbgUI.Text(string.Format("IAS:   %1 km/h", Math.Round(m_fDbgIAS)));
			DbgUI.Text(string.Format("VS:    %1 m/s", Math.Round(m_fDbgVS * 10) * 0.1));
			DbgUI.Text(string.Format("G Nz:  %1", Math.Round(m_fDbgGNormal * 100) * 0.01));
			DbgUI.Text(string.Format("G Lat: %1", Math.Round(m_fDbgGLateral * 100) * 0.01));
			DbgUI.Text(string.Format("G Lon: %1", Math.Round(m_fDbgGLongitudinal * 100) * 0.01));
			DbgUI.Text(string.Format("Wind:  %1 m/s", Math.Round(m_vWindWS.Length() * 10) * 0.1));
			DbgUI.End();
		}

		vector netForce = vector.Zero;
		for (int i = 0; i < m_aDebugForces.Count(); i++)
			netForce += m_aDebugForces[i];

		float netLiftN = netForce[1];
		if (Math.AbsFloat(netLiftN) > 100)
		{
			float liftArrowScale = 0.00005;
			vector liftTip = origin + Vector(0, netLiftN * liftArrowScale, 0);
			int liftColor = ARGB(255, 200, 200, 255);
			Shape.CreateArrow(origin, liftTip, 0.25, liftColor, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			DebugTextWorldSpace.Create(world,
				string.Format("lift: %1 kN", Math.Round(netLiftN / 1000)),
				TEXT_FLAGS, liftTip[0], liftTip[1] + 0.5, liftTip[2],
				TEXT_SIZE, TEXT_COLOR, TEXT_BG);
		}

		float forceScale = 0.0005;
		for (int i = 0; i < m_aSurfaces.Count(); i++)
		{
			PFC_AeroSurface s = m_aSurfaces[i];

			vector pos = owner.CoordToParent(s.GetLocalPosition());

			vector normalWS  = owner.VectorToParent(s.GetLocalNormal());
			vector forwardWS = owner.VectorToParent(s.GetLocalForward());

			DrawSurfaceExtent(owner, s, pos, forwardWS, normalWS);

			if (i < m_aDebugForces.Count())
			{
				vector force = m_aDebugForces[i];
				if (force.LengthSq() > 1.0)
					Shape.CreateArrow(pos, pos + force * forceScale, 0.1, Color.GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			}
		}
	}

	protected void DrawSurfaceExtent(IEntity owner, PFC_AeroSurface s, vector center, vector forwardWS, vector normalWS)
	{
		PFC_AeroSurfaceConfig cfg = s.GetConfig();
		if (!cfg)
			return;

		float halfChord = cfg.m_fChord * 0.5;
		float halfSpan  = cfg.m_fSpan  * 0.5;

		vector spanDir = (normalWS * forwardWS).Normalized();
		if (spanDir.LengthSq() < 0.0001)
			return;

		vector c0 = center + forwardWS * halfChord + spanDir * halfSpan;
		vector c1 = center + forwardWS * halfChord - spanDir * halfSpan;
		vector c2 = center - forwardWS * halfChord - spanDir * halfSpan;
		vector c3 = center - forwardWS * halfChord + spanDir * halfSpan;

		vector tris[6];
		tris[0] = c0; tris[1] = c1; tris[2] = c2;
		tris[3] = c0; tris[4] = c2; tris[5] = c3;
		int fillColor = ARGB(80, 255, 0, 255);
		Shape.CreateTris(fillColor, ShapeFlags.ONCE | ShapeFlags.TRANSP | ShapeFlags.DOUBLESIDE | ShapeFlags.NOZBUFFER, tris, 2);

		vector lines[8];
		lines[0] = c0; lines[1] = c1;
		lines[2] = c1; lines[3] = c2;
		lines[4] = c2; lines[5] = c3;
		lines[6] = c3; lines[7] = c0;
		Shape.CreateLines(Color.MAGENTA, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, lines, 4);
	}

	protected float ComputeAltitudeAGL(IEntity owner)
	{
		BaseWorld world = owner.GetWorld();
		if (!world)
			return 0;

		const float MAX_TRACE_M = 10000.0;
		TraceParam trace = new TraceParam();
		vector worldPos = owner.GetOrigin();
		trace.Start = worldPos;
		trace.End = trace.Start - Vector(0, MAX_TRACE_M, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ref array<IEntity> exclude = {owner};
		trace.ExcludeArray = exclude;
		float traced = world.TraceMove(trace, null) * MAX_TRACE_M;
		if (traced < MAX_TRACE_M - 1)
			return traced;

		return SafeNum(worldPos[1] - world.GetSurfaceY(worldPos[0], worldPos[2]), worldPos[1]);
	}

	static float SafeNum(float v, float fallback = 0)
	{
		if (v != v || v > 1e9 || v < -1e9)
			return fallback;
		return v;
	}

	static bool BadVec(vector v)
	{
		return v[0] != v[0] || v[1] != v[1] || v[2] != v[2] || v.LengthSq() > 1e12;
	}

	protected void UpdateSignals(IEntity owner, float speed, float altitude, float altitudeAGL, float verticalSpeed, float groundSpeedHoriz, float throttle, float pitch, float roll, float yaw)
	{
		if (!m_SignalsMgr)
			return;

		float maxDef = m_fMaxControlDeflectionDeg;

		if (m_iSigAirSpeed   != -1) m_SignalsMgr.SetSignalValue(m_iSigAirSpeed,   speed * 3.6);
		if (m_iSigAltitude   != -1) m_SignalsMgr.SetSignalValue(m_iSigAltitude,   altitude);
		if (m_iSigAltAGL     != -1) m_SignalsMgr.SetSignalValue(m_iSigAltAGL,     altitudeAGL);
		if (m_iSigClimbRate  != -1) m_SignalsMgr.SetSignalValue(m_iSigClimbRate,  verticalSpeed);
		if (m_iSigGroundSpeed != -1) m_SignalsMgr.SetSignalValue(m_iSigGroundSpeed, groundSpeedHoriz * 3.6);
		if (m_iSigGLoad != -1) m_SignalsMgr.SetSignalValue(m_iSigGLoad, m_fGLoad);
		if (m_fMaxRPM > 0)
		{
			float rpmNorm = m_fEngineRPM / m_fMaxRPM;
			float audioRPM = 0;
			if (m_fEngineRPM > 1)
				audioRPM = 0.45 + rpmNorm * 0.55;
			if (m_iSigRPM != -1) m_SignalsMgr.SetSignalValue(m_iSigRPM, rpmNorm);
			if (m_iSigRotorRPMScaled != -1) m_SignalsMgr.SetSignalValue(m_iSigRotorRPMScaled, audioRPM);
		}
		if (m_fMaxRPM > 0)
		{
			for (int i = 0; i < m_aSigEngineRPMs.Count() && i < m_aEngineRPMs.Count(); i++)
			{
				int rpmSigId = m_aSigEngineRPMs[i];
				if (rpmSigId != -1)
					m_SignalsMgr.SetSignalValue(rpmSigId, m_aEngineRPMs[i] / m_fMaxRPM);
			}
		}
		if (m_iSigElevator   != -1) m_SignalsMgr.SetSignalValue(m_iSigElevator,   -pitch * maxDef);
		if (m_iSigRudder     != -1) m_SignalsMgr.SetSignalValue(m_iSigRudder,     -yaw  * maxDef);
		if (m_iSigAileron    != -1) m_SignalsMgr.SetSignalValue(m_iSigAileron,    roll  * maxDef);
		if (m_iSigFlap       != -1) m_SignalsMgr.SetSignalValue(m_iSigFlap,       m_fCurrentFlapAngle);

		vector fwd = owner.VectorToParent(Vector(0, 0, 1));
		vector right = owner.VectorToParent(Vector(1, 0, 0));
		vector up = owner.VectorToParent(Vector(0, 1, 0));
		float pitchDeg = Math.Asin(Math.Clamp(fwd[1], -1, 1)) * Math.RAD2DEG;
		float bankDeg = 0;
		float headingDeg = 0;
		if (Math.AbsFloat(right[1]) + Math.AbsFloat(up[1]) > 0.001)
			bankDeg = Math.Atan2(-right[1], up[1]) * Math.RAD2DEG;
		if (Math.AbsFloat(fwd[0]) + Math.AbsFloat(fwd[2]) > 0.001)
		{
			headingDeg = Math.Atan2(fwd[0], fwd[2]) * Math.RAD2DEG;
			if (headingDeg < 0) headingDeg += 360;
		}

		if (pitchDeg   != pitchDeg   || pitchDeg   < -360 || pitchDeg   > 360) pitchDeg   = 0;
		if (bankDeg    != bankDeg    || bankDeg    < -360 || bankDeg    > 360) bankDeg    = 0;
		if (headingDeg != headingDeg || headingDeg < -1   || headingDeg > 361) headingDeg = 0;

		if (m_iSigPitch   != -1) m_SignalsMgr.SetSignalValue(m_iSigPitch,   pitchDeg);
		if (m_iSigBank    != -1) m_SignalsMgr.SetSignalValue(m_iSigBank,    bankDeg);
		if (m_iSigHeading != -1) m_SignalsMgr.SetSignalValue(m_iSigHeading, headingDeg);
	}

	protected float GetAirDensity(float altitudeMeters)
	{
		float density = m_fAirDensitySeaLevel - m_fDensityFalloff * altitudeMeters;
		if (density < 0.1)
			density = 0.1;
		return density;
	}

	protected void RefreshWind(IEntity owner, float timeSlice)
	{
		m_fWindRefreshTimer -= timeSlice;
		if (m_fWindRefreshTimer <= 0)
		{
			m_fWindRefreshTimer = WIND_REFRESH_INTERVAL;
			UpdateSteadyWind(owner);
		}

		m_fGustTime += timeSlice;
		m_vWindWS = m_vSteadyWindWS + ComputeGust();
	}

	protected void UpdateSteadyWind(IEntity owner)
	{
		m_vSteadyWindWS = vector.Zero;
		m_fSteadyWindSpeed = 0;
		if (m_fWindScale == 0)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(owner.GetWorld());
		if (!world)
			return;
		TimeAndWeatherManagerEntity twm = world.GetTimeAndWeatherManager();
		if (!twm)
			return;
		if (!m_WeatherSituation)
			m_WeatherSituation = new LocalWeatherSituation();
		if (!twm.TryGetCompleteLocalWeather(m_WeatherSituation, 0, owner.GetOrigin()))
			return;

		float speed = m_WeatherSituation.GetGlobalWindSpeed() * m_fWindScale;
		if (speed <= 0)
			return;

		if (m_fWindGradientHeight > 0 && m_fWindSurfaceFraction < 1.0)
		{
			float agl = ComputeAltitudeAGL(owner);
			float h = Math.Clamp(agl / m_fWindGradientHeight, 0, 1);
			speed *= m_fWindSurfaceFraction + (1.0 - m_fWindSurfaceFraction) * h;
		}
		if (speed <= 0)
			return;

		m_fSteadyWindSpeed = speed;
		float toRad = (m_WeatherSituation.GetGlobalWindDir() + 180) * Math.DEG2RAD;
		m_vSteadyWindWS = Vector(Math.Sin(toRad) * speed, 0, Math.Cos(toRad) * speed);
	}

	protected vector ComputeGust()
	{
		if (m_fGustIntensity <= 0 || m_fSteadyWindSpeed <= 0)
			return vector.Zero;

		float amp = m_fGustIntensity * m_fSteadyWindSpeed;
		float t = m_fGustTime;
		float gx = TurbAxis(t, 0.0) * amp;
		float gz = TurbAxis(t, 13.7) * amp;
		float gy = TurbAxis(t, 27.3) * amp * m_fGustVerticalScale;
		return Vector(gx, gy, gz);
	}

	protected float TurbAxis(float t, float seed)
	{
		float v = Math.Sin(t * 0.7 + seed)
		        + Math.Sin(t * 1.7 + seed * 2.3) * 0.6
		        + Math.Sin(t * 3.1 + seed * 5.1) * 0.3;
		return v / 1.9;
	}

	static bool IsFinite(float v)
	{
		return v == v && v < float.MAX && v > -float.MAX;
	}

	static bool IsFiniteVec(vector v)
	{
		return IsFinite(v[0]) && IsFinite(v[1]) && IsFinite(v[2]);
	}
}
