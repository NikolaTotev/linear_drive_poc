import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';

import 'ui/calibration_panel.dart';
import 'ui/connection_bar.dart';
import 'ui/estop_panel.dart';
import 'ui/jog_panel.dart';
import 'ui/move_panel.dart';
import 'ui/profile_panel.dart';
import 'ui/rehome_banner.dart';
import 'ui/status_panel.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Linear Drive',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.deepPurple),
        useMaterial3: true,
      ),
      home: const Scaffold(body: HomePage()),
    );
  }
}

/// Main control screen for the linear drive.
///
/// Owns a [DriveController] for the lifetime of the screen and rebuilds
/// reactively whenever it notifies listeners (new ST:/EVT: lines or
/// connection-state changes).
class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final DriveController _controller = DriveController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: _controller,
      builder: (context, _) {
        final state = _controller.state;
        final isConnected = _controller.isConnected;

        // Motion commands are only meaningful when connected and while
        // the e-stop is not latched.
        final motionEnabled = isConnected && !state.estop;

        // Surface OUT_OF_RANGE / NOT_HOMED / NOT_CALIBRATED errors near
        // the Move panel.
        final moveError = state.err != 'NONE' ? state.err : null;

        // MOVE requires a valid (homed) position; disable it while a
        // re-home is pending.
        final moveEnabled = motionEnabled && !state.needsRehome;

        return SingleChildScrollView(
          scrollDirection: Axis.vertical,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Padding(
                padding: const EdgeInsets.all(20.0),
                child: Text(
                  'Linear Drive Control',
                  textAlign: TextAlign.center,
                  style: GoogleFonts.lexend(
                    textStyle: const TextStyle(
                      color: mini_colors.notQuiteBlack,
                      fontSize: 35,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ),
              ),
              const Divider(
                height: 5,
                thickness: 2,
                indent: 42,
                endIndent: 42,
                color: mini_colors.darkRoyalPurpleHighlight,
              ),
              ConnectionBar(controller: _controller),
              EstopPanel(controller: _controller, state: state),
              RehomeBanner(
                controller: _controller,
                state: state,
                enabled: motionEnabled,
              ),
              StatusPanel(state: state),
              CalibrationPanel(
                controller: _controller,
                state: state,
                enabled: motionEnabled,
              ),
              JogPanel(
                controller: _controller,
                state: state,
                enabled: motionEnabled,
              ),
              MovePanel(
                controller: _controller,
                enabled: moveEnabled,
                errorText: moveError,
              ),
              ProfilePanel(
                controller: _controller,
                activeProfile: state.profile,
                enabled: motionEnabled,
              ),
              const SizedBox(height: 24),
            ],
          ),
        );
      },
    );
  }
}
