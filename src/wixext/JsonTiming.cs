namespace Hegsie.Wix.JsonExtension
{
	/// <summary>
	/// When a JsonFile operation runs, as authored by the On attribute and stored in the
	/// WixJsonFile.On column. Install and Uninstall are bits so that Both is their union; the
	/// scheduling custom actions test the bit for the phase they run in.
	/// </summary>
	internal enum JsonTiming
	{
		Install = 1,
		Uninstall = 2,
		Both = Install | Uninstall
	}
}
