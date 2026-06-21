import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/models/drive_state.dart';

import 'section_heading.dart';

String _stateLabel(DriveOperatingState state) {
  switch (state) {
    case DriveOperatingState.idle:
      return 'IDLE';
    case DriveOperatingState.homing:
      return 'HOMING';
    case DriveOperatingState.calibrating:
      return 'CALIBRATING';
    case DriveOperatingState.jogging:
      return 'JOGGING';
    case DriveOperatingState.moving:
      return 'MOVING';
    case DriveOperatingState.estop:
      return 'ESTOP';
    case DriveOperatingState.error:
      return 'ERROR';
    case DriveOperatingState.unknown:
      return 'UNKNOWN';
  }
}

String _profileLabel(DriveProfile profile) {
  switch (profile) {
    case DriveProfile.lin:
      return 'LIN';
    case DriveProfile.trap:
      return 'TRAP';
    case DriveProfile.sin:
      return 'SIN';
    case DriveProfile.unknown:
      return '—';
  }
}

/// Status panel: shows current operating state, position, calibrated
/// range, homed status and selected profile, driven by [DriveState].
class StatusPanel extends StatelessWidget {
  const StatusPanel({super.key, required this.state});

  final DriveState state;

  Widget _row(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4.0),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Text(
            '$label: ',
            style: const TextStyle(
              fontWeight: FontWeight.w500,
              color: mini_colors.lightPurpleGrey,
            ),
          ),
          Text(
            value,
            style: const TextStyle(
              fontWeight: FontWeight.bold,
              color: mini_colors.notQuiteBlack,
            ),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final posLabel =
        state.hasValidPosition ? '${state.posCm.toStringAsFixed(2)} cm' : '—';
    final rangeLabel = state.isCalibrated
        ? '${state.rangePulses} pulses'
        : 'not calibrated';

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Status'),
          _row('State', _stateLabel(state.state)),
          _row('Position', posLabel),
          _row('Calibrated range', rangeLabel),
          _row('Homed', state.homed ? 'Yes' : 'No'),
          _row('Profile', _profileLabel(state.profile)),
          _row('Last error', state.err),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
