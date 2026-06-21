import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/models/drive_state.dart';

/// Prominent E-STOP control.
///
/// When [DriveState.estop] is clear, shows a "SOFTWARE E-STOP" button that
/// sends `ESTOP`. When latched, shows a bold red "STOPPED" indicator and a
/// "CLEAR E-STOP" button that sends `ECLR`. This reflects both software
/// and physical-button e-stop (the device reports `estop=1` for both).
class EstopPanel extends StatelessWidget {
  const EstopPanel({super.key, required this.controller, required this.state});

  final DriveController controller;
  final DriveState state;

  @override
  Widget build(BuildContext context) {
    final latched = state.estop;

    return Padding(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        children: [
          if (latched)
            Container(
              width: double.infinity,
              padding: const EdgeInsets.symmetric(vertical: 12.0),
              margin: const EdgeInsets.only(bottom: 12.0),
              decoration: BoxDecoration(
                color: Colors.red,
                borderRadius: BorderRadius.circular(8),
              ),
              child: const Text(
                'STOPPED',
                textAlign: TextAlign.center,
                style: TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                  fontSize: 22,
                  letterSpacing: 2,
                ),
              ),
            ),
          SizedBox(
            width: double.infinity,
            height: 64,
            child: ElevatedButton(
              onPressed: () {
                if (latched) {
                  controller.clearEstop();
                } else {
                  controller.estop();
                }
              },
              style: ElevatedButton.styleFrom(
                backgroundColor: latched ? Colors.orange : Colors.red,
                foregroundColor: Colors.white,
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
              child: Text(
                latched ? 'CLEAR E-STOP' : 'SOFTWARE E-STOP',
                style: const TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 22,
                  letterSpacing: 1.5,
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
