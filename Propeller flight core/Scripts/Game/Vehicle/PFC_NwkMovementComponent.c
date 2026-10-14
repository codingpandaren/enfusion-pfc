[ComponentEditorProps(category: "GameScripted/PropFlightCore", description: "Replicates transform + velocity + pilot control state from owner (pilot) to server + proxies. Proxies render from a timestamped snapshot buffer a playout-delay in the past — jitter is absorbed by the buffer instead of becoming rubberbanding. Surface visuals derive from the interpolated control state per peer.")]
class PFC_NwkMovementComponentClass : ScriptGameComponentClass
{
}

class PFC_NwkMovementComponent : ScriptGameComponent
{
	[Attribute("50", UIWidgets.EditBox, "Milliseconds between state broadcasts (20Hz at 50ms).")]
	protected float m_fSendIntervalMs;

	[Attribute("1", UIWidgets.CheckBox, "Interpolate on remote proxies.")]
	protected bool m_bInterpolate;

	[Attribute("5.0", UIWidgets.EditBox, "Position discontinuity (m) beyond which the proxy hard-snaps instead of blending it away.")]
	protected float m_fHardSnapThresholdMeters;

	[Attribute("10.0", UIWidgets.EditBox, "Blend-out rate (1/s) for position discontinuities.")]
	protected float m_fPositionCorrectionRate;

	[Attribute("0.25", UIWidgets.EditBox, "Max extrapolation time (s) past the newest snapshot when the buffer runs dry.")]
	protected float m_fMaxExtrapolationSec;

	[Attribute("20", UIWidgets.EditBox, "Rotation discontinuity (degrees) beyond which the proxy hard-snaps.")]
	protected float m_fHardSnapRotationDeg;

	[Attribute("8.0", UIWidgets.EditBox, "Blend-out rate (1/s) for rotation discontinuities.")]
	protected float m_fRotationCorrectionRate;

	[Attribute("60", UIWidgets.EditBox, "Minimum playout delay (ms) behind the estimated sender timeline.")]
	protected float m_fPlayoutMinMs;

	[Attribute("450", UIWidgets.EditBox, "Maximum playout delay (ms).")]
	protected float m_fPlayoutMaxMs;

	[Attribute("1", UIWidgets.CheckBox, "Skip state sends while the pose is static; keepalive rate applies instead.")]
	protected bool m_bIdleSendSuppression;

	[Attribute("1000", UIWidgets.EditBox, "Keepalive interval (ms) while static.")]
	protected float m_fKeepaliveMs;

	[Attribute("1", UIWidgets.CheckBox, "Server: relay states at a reduced rate when no player is near the plane.")]
	protected bool m_bDistanceTieredRelay;

	[Attribute("0", UIWidgets.CheckBox, "Apply proxy poses by steering physics velocity toward the target instead of setting the transform; transform is only used as an emergency snap.")]
	protected bool m_bVelocitySteerApply;

	[Attribute("2", UIWidgets.EditBox, "Position error (m) beyond which the pose snaps instead of steering.")]
	protected float m_fSteerSnapMeters;

	[Attribute("14", UIWidgets.EditBox, "Position correction rate (1/s).")]
	protected float m_fSteerPosRate;

	[Attribute("8", UIWidgets.EditBox, "Rotation correction rate (1/s).")]
	protected float m_fSteerRotRate;

	protected const int MAX_SNAPSHOTS = 48;
	protected const float SENDER_RESET_MS = 2000;
	protected const float TIER_POLL_MS = 1000;
	protected const float TIER_MID_M = 3000;
	protected const float TIER_FAR_M = 8000;
	protected const float TIER_MID_INTERVAL_MS = 100;
	protected const float TIER_FAR_INTERVAL_MS = 250;

	protected IEntity m_Owner;
	protected RplComponent m_RplComponent;
	protected PFC_FlightController m_FlightController;
	protected PFC_FlightModel m_FlightModel;
	protected bool m_bRelevancyConfigured;
	protected static bool s_bNwkDiagRegistered;

	protected int m_iTimeBase;
	protected float m_fLastSendMs;

	protected ref array<float> m_aSnapT = {};
	protected ref array<vector> m_aSnapPos = {};
	protected ref array<vector> m_aSnapVel = {};
	protected ref array<vector> m_aSnapAngVel = {};
	protected ref array<float> m_aSnapQx = {};
	protected ref array<float> m_aSnapQy = {};
	protected ref array<float> m_aSnapQz = {};
	protected ref array<float> m_aSnapQw = {};
	protected ref array<vector> m_aSnapCtrl = {};
	protected ref array<vector> m_aSnapCfg = {};

	protected bool m_bCtrlStateValid;
	protected vector m_vCtrlState;
	protected vector m_vCfgState;
	protected int m_iSentCtrlPacked;
	protected int m_iSentCfgPacked;

	protected bool m_bDelayInit;
	protected float m_fDelayEstMs;
	protected float m_fJitterMs;
	protected float m_fRecvIntervalMs;

	protected bool m_bSentValid;
	protected vector m_vSentPos;
	protected float m_qSent[4];
	protected vector m_vSentVel;
	protected float m_fLastActualSendMs;
	protected float m_fRelayIntervalMs;
	protected float m_fLastTierPollMs;
	protected float m_fLastRelayMs;

	protected bool m_bApplied;
	protected vector m_vLastAppliedPos;
	protected float m_qLastApplied[4];
	protected vector m_vPosOffset;
	protected float m_qRotOffset[4];
	protected bool m_bExtrapolating;
	protected int m_iHardSnaps;
	protected int m_iAbsorbs;
	protected float m_fFrameDt;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		m_RplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_FlightController = PFC_FlightController.Cast(owner.FindComponent(PFC_FlightController));
		m_FlightModel = PFC_FlightModel.Cast(owner.FindComponent(PFC_FlightModel));

		m_iTimeBase = System.GetTickCount();
		m_fLastSendMs = -1e9;
		QuatIdentity(m_qRotOffset);

		if (!s_bNwkDiagRegistered)
		{
			s_bNwkDiagRegistered = true;
			DiagMenu.RegisterBool(SCR_DebugMenuID.PFC_DEBUG_NWK, "", "Network interp debug", "Prop Flight");
		}

		SetEventMask(owner, EntityEvent.POSTFRAME);
	}

	override event bool OnTicksOnRemoteProxy()
	{
		return true;
	}

	protected float LocalMs()
	{
		return System.GetTickCount(m_iTimeBase);
	}

	override void EOnPostFrame(IEntity owner, float timeSlice)
	{
		m_fFrameDt = timeSlice;
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

		if (m_RplComponent.IsOwner())
		{
			bool isServer = Replication.IsServer();
			float now = LocalMs();

			float interval = m_fSendIntervalMs;
			if (isServer && m_bDistanceTieredRelay)
			{
				UpdateRelayTier(now);
				if (m_fRelayIntervalMs > interval)
					interval = m_fRelayIntervalMs;
			}

			if (now - m_fLastSendMs >= interval)
			{
				m_fLastSendMs = now;

				vector mat[4];
				owner.GetTransform(mat);
				vector rot[3];
				rot[0] = mat[0];
				rot[1] = mat[1];
				rot[2] = mat[2];
				float q[4];
				Math3D.MatrixToQuat(rot, q);
				vector vel = physics.GetVelocity();
				vector angVel = physics.GetAngularVelocity();

				int ctrlPacked = 0;
				int cfgPacked = 0;
				if (m_FlightController)
					ctrlPacked = PackControls(m_FlightController.GetPitchInput(), m_FlightController.GetRollInput(), m_FlightController.GetYawInput());
				if (m_FlightModel)
					cfgPacked = PackCfg(m_FlightModel.GetCurrentFlapAngle(), m_FlightModel.GetNwkAuxVisual());

				if (m_bIdleSendSuppression && m_bSentValid && now - m_fLastActualSendMs < m_fKeepaliveMs)
				{
					if (vector.DistanceSq(mat[3], m_vSentPos) < 0.0009
						&& vel.LengthSq() < 0.0025
						&& angVel.LengthSq() < 0.0004
						&& m_vSentVel.LengthSq() < 0.0025
						&& QuatAngleDeg(q, m_qSent) < 0.25
						&& ctrlPacked == m_iSentCtrlPacked
						&& cfgPacked == m_iSentCfgPacked)
						return;
				}

				m_bSentValid = true;
				m_vSentPos = mat[3];
				m_qSent[0] = q[0];
				m_qSent[1] = q[1];
				m_qSent[2] = q[2];
				m_qSent[3] = q[3];
				m_vSentVel = vel;
				m_iSentCtrlPacked = ctrlPacked;
				m_iSentCfgPacked = cfgPacked;
				m_fLastActualSendMs = now;

				if (isServer)
					Rpc(RpcAll_BroadcastState, mat[3], q, vel, angVel, ctrlPacked, cfgPacked, now);
				else
					Rpc(RpcSrv_ReceiveState, mat[3], q, vel, angVel, ctrlPacked, cfgPacked, now);
			}
			return;
		}

		if (!m_bInterpolate)
			return;

		ApplyFromBuffer(physics, timeSlice);
	}

	// ---- receive path ----

	protected void IngestState(vector pos, float q[4], vector vel, vector angVel, int ctrlPacked, int cfgPacked, float senderMs)
	{
		if (m_RplComponent.IsOwner())
			return;

		Physics physics = m_Owner.GetPhysics();
		if (!physics)
			return;

		if (!m_bInterpolate)
		{
			m_vCtrlState = UnpackControls(ctrlPacked);
			m_vCfgState = UnpackCfg(cfgPacked);
			m_bCtrlStateValid = true;
			ApplyPose(physics, pos, q, vel, angVel);
			return;
		}

		float delay = LocalMs() - senderMs;

		// sender machine changed (ownership handoff) — its clock base is unrelated, restart mapping
		if (m_bDelayInit && Math.AbsFloat(delay - m_fDelayEstMs) > SENDER_RESET_MS)
		{
			ClearBuffer();
			m_bDelayInit = false;
		}

		if (!m_bDelayInit)
		{
			m_bDelayInit = true;
			m_fDelayEstMs = delay;
			m_fJitterMs = 0;
		}
		else
		{
			// track the least-delayed packets; slow upward drift follows clock skew / route changes
			if (delay < m_fDelayEstMs)
				m_fDelayEstMs = delay;
			else
				m_fDelayEstMs = m_fDelayEstMs + (delay - m_fDelayEstMs) * 0.002;
			m_fJitterMs = m_fJitterMs + (Math.AbsFloat(delay - m_fDelayEstMs) - m_fJitterMs) * 0.05;
		}

		int n = m_aSnapT.Count();
		if (n > 0 && senderMs <= m_aSnapT[n - 1])
			return;

		// measured spacing feeds the playout delay — throttled/keepalive senders still bracket correctly
		if (n > 0)
		{
			float dtMs = Math.Clamp(senderMs - m_aSnapT[n - 1], 10, 1500);
			if (m_fRecvIntervalMs <= 0.01)
				m_fRecvIntervalMs = dtMs;
			else
				m_fRecvIntervalMs = m_fRecvIntervalMs + (dtMs - m_fRecvIntervalMs) * 0.1;
		}

		m_aSnapT.Insert(senderMs);
		m_aSnapPos.Insert(pos);
		m_aSnapVel.Insert(vel);
		m_aSnapAngVel.Insert(angVel);
		m_aSnapQx.Insert(q[0]);
		m_aSnapQy.Insert(q[1]);
		m_aSnapQz.Insert(q[2]);
		m_aSnapQw.Insert(q[3]);
		m_aSnapCtrl.Insert(UnpackControls(ctrlPacked));
		m_aSnapCfg.Insert(UnpackCfg(cfgPacked));

		if (m_aSnapT.Count() > MAX_SNAPSHOTS)
			RemoveSnapshot(0);
	}

	protected void RemoveSnapshot(int idx)
	{
		m_aSnapT.RemoveOrdered(idx);
		m_aSnapPos.RemoveOrdered(idx);
		m_aSnapVel.RemoveOrdered(idx);
		m_aSnapAngVel.RemoveOrdered(idx);
		m_aSnapQx.RemoveOrdered(idx);
		m_aSnapQy.RemoveOrdered(idx);
		m_aSnapQz.RemoveOrdered(idx);
		m_aSnapQw.RemoveOrdered(idx);
		m_aSnapCtrl.RemoveOrdered(idx);
		m_aSnapCfg.RemoveOrdered(idx);
	}

	protected void ClearBuffer()
	{
		m_aSnapT.Clear();
		m_aSnapPos.Clear();
		m_aSnapVel.Clear();
		m_aSnapAngVel.Clear();
		m_aSnapQx.Clear();
		m_aSnapQy.Clear();
		m_aSnapQz.Clear();
		m_aSnapQw.Clear();
		m_aSnapCtrl.Clear();
		m_aSnapCfg.Clear();
	}

	protected void GetSnapQuat(int idx, out float q[4])
	{
		q[0] = m_aSnapQx[idx];
		q[1] = m_aSnapQy[idx];
		q[2] = m_aSnapQz[idx];
		q[3] = m_aSnapQw[idx];
	}

	// ---- proxy render path ----

	protected void ApplyFromBuffer(Physics physics, float timeSlice)
	{
		int n = m_aSnapT.Count();
		if (n == 0 || !m_bDelayInit)
			return;

		float baseIntervalMs = m_fSendIntervalMs;
		if (m_fRecvIntervalMs > baseIntervalMs)
			baseIntervalMs = m_fRecvIntervalMs;
		float playout = Math.Clamp(baseIntervalMs * 1.5 + m_fJitterMs * 3.0, m_fPlayoutMinMs, m_fPlayoutMaxMs);
		float rt = LocalMs() - m_fDelayEstMs - playout;

		// drop snapshots the render time has moved past (keep one behind for the bracket)
		while (m_aSnapT.Count() > 2 && m_aSnapT[1] < rt)
			RemoveSnapshot(0);
		n = m_aSnapT.Count();

		vector targetPos;
		vector targetVel;
		vector targetAngVel;
		float qTarget[4];

		if (rt <= m_aSnapT[0])
		{
			// waiting for the buffer to fill (fresh spawn) — hold still so the absorber stays quiet
			targetPos = m_aSnapPos[0];
			targetVel = vector.Zero;
			targetAngVel = vector.Zero;
			GetSnapQuat(0, qTarget);
			m_vCtrlState = m_aSnapCtrl[0];
			m_vCfgState = m_aSnapCfg[0];
			m_bExtrapolating = false;
		}
		else if (rt >= m_aSnapT[n - 1])
		{
			// full-velocity extrapolation up to the cap, then velocity fades to zero instead of
			// freezing — a hard freeze makes the absorber chase its own prediction and balloon
			float e = (rt - m_aSnapT[n - 1]) * 0.001;
			float decDur = m_fMaxExtrapolationSec * 2.0;
			float effTime;
			float velScale;
			if (e <= m_fMaxExtrapolationSec)
			{
				effTime = e;
				velScale = 1.0;
			}
			else
			{
				float tDec = Math.Min(e - m_fMaxExtrapolationSec, decDur);
				velScale = 1.0 - tDec / decDur;
				effTime = m_fMaxExtrapolationSec + tDec - tDec * tDec / (2.0 * decDur);
			}
			vector velN = m_aSnapVel[n - 1];
			vector angVelN = m_aSnapAngVel[n - 1];
			targetPos = m_aSnapPos[n - 1] + velN * effTime;
			targetVel = velN * velScale;
			targetAngVel = angVelN * velScale;
			float qBase[4];
			GetSnapQuat(n - 1, qBase);
			IntegrateQuat(qBase, angVelN, effTime, qTarget);
			m_vCtrlState = m_aSnapCtrl[n - 1];
			m_vCfgState = m_aSnapCfg[n - 1];
			m_bExtrapolating = true;
		}
		else
		{
			int hi = n - 1;
			while (hi > 1 && m_aSnapT[hi - 1] > rt)
				hi--;
			int lo = hi - 1;

			float segMs = m_aSnapT[hi] - m_aSnapT[lo];
			float u = 0;
			if (segMs > 0.01)
				u = (rt - m_aSnapT[lo]) / segMs;
			float segSec = segMs * 0.001;

			targetPos = HermitePos(m_aSnapPos[lo], m_aSnapVel[lo], m_aSnapPos[hi], m_aSnapVel[hi], segSec, u);
			targetVel = vector.Lerp(m_aSnapVel[lo], m_aSnapVel[hi], u);
			targetAngVel = vector.Lerp(m_aSnapAngVel[lo], m_aSnapAngVel[hi], u);

			float qA[4], qB[4];
			GetSnapQuat(lo, qA);
			GetSnapQuat(hi, qB);
			Math3D.QuatLerp(qTarget, qA, qB, u);
			m_vCtrlState = vector.Lerp(m_aSnapCtrl[lo], m_aSnapCtrl[hi], u);
			m_vCfgState = vector.Lerp(m_aSnapCfg[lo], m_aSnapCfg[hi], u);
			m_bExtrapolating = false;
		}
		m_bCtrlStateValid = true;

		if (!m_bApplied)
		{
			m_bApplied = true;
			m_vPosOffset = vector.Zero;
			QuatIdentity(m_qRotOffset);
			ApplyPose(physics, targetPos, qTarget, targetVel, targetAngVel);
			return;
		}

		// continuity absorber: any target jump the last applied pose could not have reached this
		// frame (loss recovery, extrapolation correction) is folded into a decaying offset
		vector predicted = m_vLastAppliedPos + targetVel * timeSlice;
		vector newApplied = targetPos + m_vPosOffset;
		vector disc = newApplied - predicted;
		float tol = 0.35 + targetVel.Length() * timeSlice * 0.5;
		if (disc.LengthSq() > tol * tol)
		{
			m_vPosOffset = m_vPosOffset - disc;
			m_iAbsorbs++;
		}

		float qCandidate[4];
		Math3D.QuatMultiply(qCandidate, m_qRotOffset, qTarget);
		float rotDiscDeg = QuatAngleDeg(qCandidate, m_qLastApplied);
		float rotTolDeg = 3.0 + targetAngVel.Length() * Math.RAD2DEG * timeSlice * 1.5;
		if (rotDiscDeg > rotTolDeg)
		{
			float qInv[4];
			QuatConjugate(qTarget, qInv);
			Math3D.QuatMultiply(m_qRotOffset, m_qLastApplied, qInv);
			// keep w positive so the identity-lerp decay takes the short way around
			if (m_qRotOffset[3] < 0)
			{
				m_qRotOffset[0] = -m_qRotOffset[0];
				m_qRotOffset[1] = -m_qRotOffset[1];
				m_qRotOffset[2] = -m_qRotOffset[2];
				m_qRotOffset[3] = -m_qRotOffset[3];
			}
			m_iAbsorbs++;
		}

		// hard snap if the accumulated offset is beyond blending
		if (m_vPosOffset.LengthSq() > m_fHardSnapThresholdMeters * m_fHardSnapThresholdMeters || QuatOffsetAngleDeg(m_qRotOffset) > m_fHardSnapRotationDeg)
		{
			m_vPosOffset = vector.Zero;
			QuatIdentity(m_qRotOffset);
			m_iHardSnaps++;
		}

		// decay offsets toward zero
		float kPos = Math.Clamp(m_fPositionCorrectionRate * timeSlice, 0, 1);
		m_vPosOffset = m_vPosOffset * (1.0 - kPos);
		float kRot = Math.Clamp(m_fRotationCorrectionRate * timeSlice, 0, 1);
		float qIdent[4];
		QuatIdentity(qIdent);
		Math3D.QuatLerp(m_qRotOffset, m_qRotOffset, qIdent, kRot);

		float qFinal[4];
		Math3D.QuatMultiply(qFinal, m_qRotOffset, qTarget);
		ApplyPose(physics, targetPos + m_vPosOffset, qFinal, targetVel, targetAngVel);

		if (DiagMenu.GetBool(SCR_DebugMenuID.PFC_DEBUG_NWK))
			DrawNwkDebug(playout);
	}

	protected void ApplyPose(Physics physics, vector pos, float q[4], vector vel, vector angVel)
	{
		if (m_bVelocitySteerApply && m_bApplied && m_fFrameDt > 0.0001)
		{
			vector curMat[4];
			m_Owner.GetTransform(curMat);
			vector posErr = pos - curMat[3];
			float posErrLen = posErr.Length();
			if (posErrLen <= m_fSteerSnapMeters)
			{
				float qCur[4];
				Math3D.MatrixToQuat(curMat, qCur);
				float qConj[4];
				qConj[0] = -qCur[0];
				qConj[1] = -qCur[1];
				qConj[2] = -qCur[2];
				qConj[3] = qCur[3];
				float qErr[4];
				Math3D.QuatMultiply(qErr, q, qConj);
				if (qErr[3] < 0)
				{
					qErr[0] = -qErr[0];
					qErr[1] = -qErr[1];
					qErr[2] = -qErr[2];
					qErr[3] = -qErr[3];
				}
				float halfAngle = Math.Acos(Math.Clamp(qErr[3], -1, 1));
				float angle = 2 * halfAngle;
				if (angle <= 0.45)
				{
					float invDt = 1.0 / m_fFrameDt;

					float kPos = m_fSteerPosRate;
					if (kPos > invDt)
						kPos = invDt;
					vector velCorr = posErr * kPos;
					if (posErrLen < 0.04)
						velCorr = vector.Zero;
					float velCorrLen = velCorr.Length();
					if (velCorrLen > 40)
						velCorr = velCorr * (40 / velCorrLen);
					physics.SetVelocity(vel + velCorr);

					float kRot = m_fSteerRotRate;
					if (kRot > invDt)
						kRot = invDt;
					vector angCorr = vector.Zero;
					float sinHalf = Math.Sin(halfAngle);
					if (sinHalf > 0.0001 && angle > 0.008)
						angCorr = Vector(qErr[0] / sinHalf, qErr[1] / sinHalf, qErr[2] / sinHalf) * (angle * kRot);
					float angCorrLen = angCorr.Length();
					if (angCorrLen > 3)
						angCorr = angCorr * (3 / angCorrLen);
					physics.SetAngularVelocity(angVel + angCorr);

					m_vLastAppliedPos = pos;
					m_qLastApplied[0] = q[0];
					m_qLastApplied[1] = q[1];
					m_qLastApplied[2] = q[2];
					m_qLastApplied[3] = q[3];
					return;
				}
			}
		}

		vector rot[3];
		Math3D.QuatToMatrix(q, rot);
		vector mat[4];
		mat[0] = rot[0];
		mat[1] = rot[1];
		mat[2] = rot[2];
		mat[3] = pos;
		m_Owner.SetTransform(mat);
		physics.SetVelocity(vel);
		physics.SetAngularVelocity(angVel);

		m_vLastAppliedPos = pos;
		m_qLastApplied[0] = q[0];
		m_qLastApplied[1] = q[1];
		m_qLastApplied[2] = q[2];
		m_qLastApplied[3] = q[3];
	}

	protected void DrawNwkDebug(float playout)
	{
		string extra = "interp";
		if (m_bExtrapolating)
			extra = "EXTRAP";
		DbgUI.Begin(string.Format("PFC Nwk %1", m_Owner));
		DbgUI.Text(string.Format("buffer %1  delay %2 ms  jitter %3 ms", m_aSnapT.Count(), m_fDelayEstMs.ToString(6, 0), m_fJitterMs.ToString(5, 1)));
		DbgUI.Text(string.Format("playout %1 ms  mode %2", playout.ToString(5, 0), extra));
		DbgUI.Text(string.Format("posOff %1 m  absorbs %2  snaps %3", m_vPosOffset.Length().ToString(5, 2), m_iAbsorbs, m_iHardSnaps));
		DbgUI.End();
	}

	bool GetProxyControlState(out vector ctrl, out vector cfg)
	{
		ctrl = m_vCtrlState;
		cfg = m_vCfgState;
		return m_bCtrlStateValid;
	}

	protected static int PackControls(float pitch, float roll, float yaw)
	{
		int p = Math.Round((Math.Clamp(pitch, -1, 1) + 1.0) * 511.5);
		int r = Math.Round((Math.Clamp(roll, -1, 1) + 1.0) * 511.5);
		int y = Math.Round((Math.Clamp(yaw, -1, 1) + 1.0) * 511.5);
		return p | (r << 10) | (y << 20);
	}

	protected static vector UnpackControls(int packed)
	{
		int hi = packed / 1024;
		int pi = packed - hi * 1024;
		int yi = hi / 1024;
		int ri = hi - yi * 1024;
		return Vector((pi / 511.5) - 1.0, (ri / 511.5) - 1.0, (yi / 511.5) - 1.0);
	}

	protected static int PackCfg(float flapDeg, float aux01)
	{
		int f = Math.Round(Math.Clamp(flapDeg, 0, 102.3) * 10);
		int a = Math.Round(Math.Clamp(aux01, 0, 1) * 1023);
		return f | (a << 10);
	}

	protected static vector UnpackCfg(int packed)
	{
		int ai = packed / 1024;
		int fi = packed - ai * 1024;
		return Vector(fi * 0.1, ai / 1023.0, 0);
	}

	protected void UpdateRelayTier(float now)
	{
		if (now - m_fLastTierPollMs < TIER_POLL_MS)
			return;
		m_fLastTierPollMs = now;
		m_fRelayIntervalMs = 0;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> playerIds = {};
		pm.GetPlayers(playerIds);

		float bestSq = -1;
		vector here = m_Owner.GetOrigin();
		foreach (int pid : playerIds)
		{
			IEntity controlled = pm.GetPlayerControlledEntity(pid);
			if (!controlled)
				continue;
			float dSq = vector.DistanceSq(controlled.GetOrigin(), here);
			if (bestSq < 0 || dSq < bestSq)
				bestSq = dSq;
		}

		if (bestSq < 0 || bestSq > TIER_FAR_M * TIER_FAR_M)
			m_fRelayIntervalMs = TIER_FAR_INTERVAL_MS;
		else if (bestSq > TIER_MID_M * TIER_MID_M)
			m_fRelayIntervalMs = TIER_MID_INTERVAL_MS;
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcSrv_ReceiveState(vector pos, float q[4], vector vel, vector angVel, int ctrlPacked, int cfgPacked, float senderMs)
	{
		IngestState(pos, q, vel, angVel, ctrlPacked, cfgPacked, senderMs);

		float now = LocalMs();
		if (m_bDistanceTieredRelay)
		{
			UpdateRelayTier(now);
			if (m_fRelayIntervalMs > 0 && now - m_fLastRelayMs < m_fRelayIntervalMs)
				return;
		}
		m_fLastRelayMs = now;
		Rpc(RpcAll_BroadcastState, pos, q, vel, angVel, ctrlPacked, cfgPacked, senderMs);
	}

	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcAll_BroadcastState(vector pos, float q[4], vector vel, vector angVel, int ctrlPacked, int cfgPacked, float senderMs)
	{
		IngestState(pos, q, vel, angVel, ctrlPacked, cfgPacked, senderMs);
	}

	// ---- math helpers ----

	protected static vector HermitePos(vector p0, vector v0, vector p1, vector v1, float segSec, float u)
	{
		float u2 = u * u;
		float u3 = u2 * u;
		float h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
		float h10 = u3 - 2.0 * u2 + u;
		float h01 = -2.0 * u3 + 3.0 * u2;
		float h11 = u3 - u2;
		return p0 * h00 + v0 * (h10 * segSec) + p1 * h01 + v1 * (h11 * segSec);
	}

	protected static void QuatIdentity(out float q[4])
	{
		q[0] = 0;
		q[1] = 0;
		q[2] = 0;
		q[3] = 1;
	}

	protected static void QuatConjugate(float q[4], out float qOut[4])
	{
		qOut[0] = -q[0];
		qOut[1] = -q[1];
		qOut[2] = -q[2];
		qOut[3] = q[3];
	}

	protected static float QuatAngleDeg(float a[4], float b[4])
	{
		float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
		dot = Math.AbsFloat(dot);
		if (dot > 1)
			dot = 1;
		return 2.0 * Math.Acos(dot) * Math.RAD2DEG;
	}

	protected static float QuatOffsetAngleDeg(float q[4])
	{
		float w = Math.AbsFloat(q[3]);
		if (w > 1)
			w = 1;
		return 2.0 * Math.Acos(w) * Math.RAD2DEG;
	}

	protected static void IntegrateQuat(float qBase[4], vector angVel, float dt, out float qOut[4])
	{
		float angle = angVel.Length() * dt;
		if (angle < 0.0001)
		{
			qOut[0] = qBase[0];
			qOut[1] = qBase[1];
			qOut[2] = qBase[2];
			qOut[3] = qBase[3];
			return;
		}
		vector axis = angVel.Normalized();
		float half = angle * 0.5;
		float s = Math.Sin(half);
		float qRot[4];
		qRot[0] = axis[0] * s;
		qRot[1] = axis[1] * s;
		qRot[2] = axis[2] * s;
		qRot[3] = Math.Cos(half);
		Math3D.QuatMultiply(qOut, qRot, qBase);
	}
}
