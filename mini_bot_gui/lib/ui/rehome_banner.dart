import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/models/drive_state.dart';

/// Banner shown when an `EVT:LIMIT` (unexpected endstop hit) has occurred
/// and the device requires a re-home. Offers the Home button directly;
/// cleared automatically once [DriveState.homed] becomes true again
/// (i.e. after `EVT:HOMED`).
class RehomeBanner extends StatelessWidget {
  const RehomeBanner({
    super.key,
    required this.controller,
    required this.state,
    required this.enabled,
  });

  final DriveController controller;
  final DriveState state;

  /// Whether the Home command may be sent (e.g. false while e-stop
  /// latched).
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    if (!state.needsRehome) {
      return const SizedBox.shrink();
    }

    final side = switch (state.limitSide) {
      LimitSide.min => 'MIN',
      LimitSide.max => 'MAX',
      LimitSide.unknown => 'an endstop',
    };

    return Container(
      width: double.infinity,
      margin: const EdgeInsets.all(16.0),
      padding: const EdgeInsets.all(16.0),
      decoration: BoxDecoration(
        color: Colors.red.shade100,
        border: Border.all(color: Colors.red, width: 2),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        children: [
          Text(
            'Unexpected limit hit ($side) — please re-home',
            textAlign: TextAlign.center,
            style: const TextStyle(
              color: Colors.red,
              fontWeight: FontWeight.bold,
              fontSize: 16,
            ),
          ),
          const SizedBox(height: 12),
          ElevatedButton(
            onPressed: enabled ? controller.home : null,
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.red,
              foregroundColor: Colors.white,
            ),
            child: const Text('Home'),
          ),
        ],
      ),
    );
  }
}
