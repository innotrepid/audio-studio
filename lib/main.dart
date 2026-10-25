import 'package:flutter/material.dart';

void main() {
  runApp(const AudioStudioApp());
}

/// Initial shell for the Audio Studio cross-platform application.
///
/// Audio capture and processing are intentionally not implemented here yet.
/// They will be introduced behind a dedicated service boundary after the
/// Android and Windows capture options have been validated.
class AudioStudioApp extends StatelessWidget {
  const AudioStudioApp({super.key});

  @override
  Widget build(BuildContext context) {
    const seedColor = Color(0xFF7C9AFF);

    return MaterialApp(
      title: 'Audio Studio',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(
          seedColor: seedColor,
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xFF101218),
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF101218),
          surfaceTintColor: Colors.transparent,
        ),
        cardTheme: const CardThemeData(
          color: Color(0xFF1A1E28),
          margin: EdgeInsets.zero,
        ),
      ),
      home: const StudioHomePage(),
    );
  }
}

class StudioHomePage extends StatelessWidget {
  const StudioHomePage({super.key});

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(
        title: const Row(
          children: [
            Icon(Icons.graphic_eq_rounded),
            SizedBox(width: 10),
            Text('Audio Studio'),
          ],
        ),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: Center(
              child: Chip(
                avatar: Icon(
                  Icons.construction_rounded,
                  size: 16,
                  color: colorScheme.primary,
                ),
                label: const Text('Foundation'),
              ),
            ),
          ),
        ],
      ),
      body: SafeArea(
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 880),
            child: ListView(
              padding: const EdgeInsets.all(24),
              children: [
                Text(
                  'Your voice. Your studio.',
                  style: Theme.of(context).textTheme.headlineLarge?.copyWith(
                        fontWeight: FontWeight.w700,
                      ),
                ),
                const SizedBox(height: 10),
                Text(
                  'A calm workspace for recording, cleaning, and producing '
                  'your narration. We are building the reliable audio '
                  'foundation first.',
                  style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                        height: 1.5,
                      ),
                ),
                const SizedBox(height: 28),
                const _FoundationNotice(),
                const SizedBox(height: 20),
                const _FeatureCard(
                  icon: Icons.mic_none_rounded,
                  title: 'Studio',
                  description:
                      'Reliable microphone capture, input monitoring, and '
                      'safe recording finalization.',
                  status: 'First milestone',
                ),
                const SizedBox(height: 12),
                const _FeatureCard(
                  icon: Icons.graphic_eq_rounded,
                  title: 'Audio cleanup',
                  description:
                      'Evaluate noise reduction and room-echo reduction '
                      'without sacrificing natural speech.',
                  status: 'Engine research',
                ),
                const SizedBox(height: 12),
                const _FeatureCard(
                  icon: Icons.subject_rounded,
                  title: 'Script and takes',
                  description:
                      'Keep scripts, takes, and targeted retakes together '
                      'in one project.',
                  status: 'Planned',
                ),
                const SizedBox(height: 12),
                const _FeatureCard(
                  icon: Icons.movie_creation_outlined,
                  title: 'Creator tools',
                  description:
                      'Build toward captions, narration timing, and '
                      'production markers.',
                  status: 'Planned',
                ),
                const SizedBox(height: 28),
                Text(
                  'Designed for Android and Windows',
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.titleMedium,
                ),
                const SizedBox(height: 6),
                Text(
                  'Core projects stay local by default. Cloud AI will be '
                  'optional, not required for recording.',
                  textAlign: TextAlign.center,
                  style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                        color: colorScheme.onSurfaceVariant,
                      ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _FoundationNotice extends StatelessWidget {
  const _FoundationNotice();

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: colorScheme.primaryContainer.withValues(alpha: 0.35),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(
          color: colorScheme.primary.withValues(alpha: 0.35),
        ),
      ),
      child: const Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(Icons.info_outline_rounded),
          SizedBox(width: 12),
          Expanded(
            child: Text(
              'This is the initial app shell. Recording and audio processing '
              'will be added only after the platform capture path is '
              'validated. No recording controls are shown until they work.',
            ),
          ),
        ],
      ),
    );
  }
}

class _FeatureCard extends StatelessWidget {
  const _FeatureCard({
    required this.icon,
    required this.title,
    required this.description,
    required this.status,
  });

  final IconData icon;
  final String title;
  final String description;
  final String status;

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              width: 44,
              height: 44,
              decoration: BoxDecoration(
                color: colorScheme.primaryContainer,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(
                icon,
                color: colorScheme.onPrimaryContainer,
              ),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    title,
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                          fontWeight: FontWeight.w600,
                        ),
                  ),
                  const SizedBox(height: 5),
                  Text(
                    description,
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                          color: colorScheme.onSurfaceVariant,
                          height: 1.4,
                        ),
                  ),
                  const SizedBox(height: 10),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      status,
                      style: Theme.of(context).textTheme.labelMedium?.copyWith(
                            color: colorScheme.primary,
                          ),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
