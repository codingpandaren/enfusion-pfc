[ComponentEditorProps(category: "GameScripted/PropFlightCore", description: "Replicates transform + velocity from owner (pilot) to server + proxies. Velocity-driven smoothing on proxies; hard snap fallback for large drift.")]
class PFC_NwkMovementComponentClass : ScriptGameComponentClass
{
}

class PFC_NwkMovementComponent : ScriptGameComponent
{
	[Attribute("50", UIWidgets.EditBox, "Milliseconds between state broadcasts (20Hz at 50ms).")]
	protected float m_fSendIntervalMs;

	[Attribute("1", UIWidgets.CheckBox, "Interpolate on remote proxies.")]
	protected bool m_bInterpolate;

	[Attribute("5.0", UIWidgets.EditBox, "Position drift (m) threshold for hard snap vs soft correction.")]
	protected float m_fHardSnapThresholdMeters;

	[Attribute("10.0", UIWidgets.EditBox, "Position correction rate (1/s) for soft-mode velocity bias.")]
	protected float m_fPositionCorrectionRate;

	[Attribute("0.05", UIWidgets.EditBox, "Max extrapolation time (seconds) from last received sample.")]
	protected float m_fMaxExtrapolationSec;

	[Attribute("20", UIWidgets.EditBox, "Rotation-error threshold (degrees) for a hard snap — catches a freshly streamed-in / JIP proxy that's facing the wrong way.")]
	protected float m_fHardSnapRotationDeg;

	[Attribute("8.0", UIWidgets.EditBox, "Rotation correction rate (1/s) for the soft-mode angular-velocity bias toward the target orientation.")]
	protected float m_fRotationCorrectionRate;

	protected IEntity m_Owner;
	protected RplComponent m_RplComponent;

	protected float m_fHardSnapRotCosHalf = -1;

	protected float m_fLastSendMs;

	protected vector m_PrevState[4];
	protected vector m_PrevVel;
	protected vector m_PrevAngVel;
	protected float m_fPrevStateMs;

	protected vector m_CurState[4];
	protected vector m_CurVel;
	protected vector m_CurAngVel;
	protected float m_fCurStateMs;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		owner.GetTransform(m_PrevState);
		owner.GetTransform(m_CurState);

		float now = System.GetTickCount();
		m_fPrevStateMs = now;
		m_fCurStateMs = now;
		m_fLastSendMs = -1e9;

		m_fHardSnapRotCosHalf = Math.Cos(m_fHardSnapRotationDeg * Math.DEG2RAD * 0.5);

		SetEventMask(owner, EntityEvent.POSTFRAME);
	}

	protected bool m_bRelevancyConfigured;

	override event bool OnTicksOnRemoteProxy()
	{
		return true;
	}

	override void EOnPostFrame(IEntity owner, float timeSlice)
	{
		if (!m_RplComponent)
			return;

		if (!m_RplComponent.Id().IsValid())
			return;

		if (!m_bRelevancyConfigured)
		{
			m_bRelevancyConfigured = true;
			m_RplComponent.EnableSpatialRelevancy(false);
			m_RplComponent.EnableStreaming(false);
		}

		Physics physics = owner.GetPhysics();
		if (!physics)
			return;

		float now = System.GetTickCount();

		if (m_RplComponent.IsOwner())
		{
			if (now - m_fLastSendMs >= m_fSendIntervalMs)
			{
				vector mat[4];
				owner.GetTransform(mat);
				vector vel = physics.GetVelocity();
				vector angVel = physics.GetAngularVelocity();
				if (Replication.IsServer())
					Rpc(RpcAll_BroadcastState, mat, vel, angVel);
				else
					Rpc(RpcSrv_ReceiveState, mat, vel, angVel);
				m_fLastSendMs = now;
			}
			return;
		}

		if (!m_bInterpolate || m_fCurStateMs == m_fPrevStateMs)
			return;

		float t = (now - m_fCurStateMs) / m_fSendIntervalMs;
		t = Math.Clamp(t, 0, 1);

		ApplyInterpolated(physics, t);
	}

	protected void ApplyInterpolated(Physics physics, float t)
	{
		float secSinceCur = (System.GetTickCount() - m_fCurStateMs) * 0.001;
		if (secSinceCur < 0) secSinceCur = 0;
		if (secSinceCur > m_fMaxExtrapolationSec)
			secSinceCur = m_fMaxExtrapolationSec;

		vector targetPos    = m_CurState[3] + m_CurVel * secSinceCur;
		vector targetVel    = m_CurVel;
		vector targetAngVel = m_CurAngVel;

		vector prevRot[3];
		prevRot[0] = m_PrevState[0];
		prevRot[1] = m_PrevState[1];
		prevRot[2] = m_PrevState[2];

		vector curRot[3];
		curRot[0] = m_CurState[0];
		curRot[1] = m_CurState[1];
		curRot[2] = m_CurState[2];

		float qPrev[4], qCur[4], qTarget[4];
		Math3D.MatrixToQuat(prevRot, qPrev);
		Math3D.MatrixToQuat(curRot, qCur);
		Math3D.QuatLerp(qTarget, qPrev, qCur, t);

		vector targetRot[3];
		Math3D.QuatToMatrix(qTarget, targetRot);

		vector nowMat[4];
		m_Owner.GetTransform(nowMat);
		vector nowRot[3];
		nowRot[0] = nowMat[0];
		nowRot[1] = nowMat[1];
		nowRot[2] = nowMat[2];
		float qNow[4];
		Math3D.MatrixToQuat(nowRot, qNow);

		float rotDot = qNow[0] * qTarget[0] + qNow[1] * qTarget[1] + qNow[2] * qTarget[2] + qNow[3] * qTarget[3];
		rotDot = Math.AbsFloat(rotDot);

		vector currentPos = nowMat[3];
		vector posError = targetPos - currentPos;
		float driftSq = posError.LengthSq();
		float thresholdSq = m_fHardSnapThresholdMeters * m_fHardSnapThresholdMeters;

		if (driftSq > thresholdSq || rotDot < m_fHardSnapRotCosHalf)
		{
			vector mat[4];
			mat[0] = targetRot[0];
			mat[1] = targetRot[1];
			mat[2] = targetRot[2];
			mat[3] = targetPos;
			m_Owner.SetTransform(mat);

			physics.SetVelocity(targetVel);
			physics.SetAngularVelocity(targetAngVel);
			return;
		}

		vector correctionVel = posError * m_fPositionCorrectionRate;
		vector rotErr = (CrossProduct(nowRot[0], targetRot[0]) + CrossProduct(nowRot[1], targetRot[1]) + CrossProduct(nowRot[2], targetRot[2])) * 0.5;

		physics.SetVelocity(targetVel + correctionVel);
		physics.SetAngularVelocity(targetAngVel + rotErr * m_fRotationCorrectionRate);
	}

	protected vector CrossProduct(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	protected void IngestState(vector mat[4], vector vel, vector angVel)
	{
		if (m_RplComponent.IsOwner())
			return;

		Physics physics = m_Owner.GetPhysics();
		if (!physics)
			return;

		if (!m_bInterpolate)
		{
			m_Owner.SetTransform(mat);
			physics.SetVelocity(vel);
			physics.SetAngularVelocity(angVel);
			return;
		}

		float now = System.GetTickCount();
		if (now != m_fCurStateMs)
		{
			m_PrevState = m_CurState;
			m_PrevVel = m_CurVel;
			m_PrevAngVel = m_CurAngVel;
			m_fPrevStateMs = m_fCurStateMs;
		}
		m_CurState = mat;
		m_CurVel = vel;
		m_CurAngVel = angVel;
		m_fCurStateMs = now;
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcSrv_ReceiveState(vector mat[4], vector vel, vector angVel)
	{
		IngestState(mat, vel, angVel);
		Rpc(RpcAll_BroadcastState, mat, vel, angVel);
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcAll_BroadcastState(vector mat[4], vector vel, vector angVel)
	{
		IngestState(mat, vel, angVel);
	}
}
