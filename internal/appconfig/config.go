// Package appconfig is a desktop app's settings file, which the CLI
// writes and the hosts read (platform/desktop's app.c, the Mac host's
// Runtime.swift).
package appconfig

// File is a desktop app's settings from its asuka.toml, which the Asuka
// CLI writes beside the executable (Windows), in a Linux prefix's
// share/<exe>, or in a Mac app's Resources: what the host shows of the app
// (its name, its accent) and how it runs (its ID, its URL scheme).
const File = "asuka-app.json"

// Config is File's content.
type Config struct {
	Name              string `json:"name,omitempty"`
	ID                string `json:"id,omitempty"`
	Version           string `json:"version,omitempty"`
	Build             string `json:"build,omitempty"`
	Accent            string `json:"accent,omitempty"`
	AccentDark        string `json:"accentDark,omitempty"`
	Scheme            string `json:"scheme,omitempty"`
	FileTypes         string `json:"fileTypes,omitempty"`
	Lang              string `json:"lang,omitempty"`
	Direction         string `json:"direction,omitempty"`
	Background        bool   `json:"background,omitempty"`
	KeepRunning       bool   `json:"keepRunning,omitempty"`
	MultipleInstances bool   `json:"multipleInstances,omitempty"`
	// The updates' feed, the public key their signatures are checked
	// with, and the build's target (platform/update).
	UpdateFeed   string `json:"updateFeed,omitempty"`
	UpdateKey    string `json:"updateKey,omitempty"`
	UpdateTarget string `json:"updateTarget,omitempty"`
	// AlternateIcons are the names of the alternate icons the
	// executable has (platform/appicon), comma-separated: icon groups
	// 201, 202 and on, in this order (Windows).
	AlternateIcons string `json:"alternateIcons,omitempty"`
}
