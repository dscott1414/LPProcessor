import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import net.htmlparser.jericho.Renderer;
import net.htmlparser.jericho.Source;

// Called by the C++ engine's Internet.cpp::runJavaJerichoHTML() as:
//   java -classpath ... RenderToText <sourceHtmlFile> <destTextFile>
// to strip HTML down to plain text (Jericho's Renderer) for the web-scrape
// pipeline. Both args are always local file paths (never a URL), and in
// the current caller sourceHtmlFile and destTextFile are the same path
// (the file is overwritten with its rendered text).
public class RenderToText {
	public static void main(String[] args) throws Exception {
		if (args.length < 2) {
			System.err.println("Usage: RenderToText <sourceHtmlFile> <destTextFile>");
			System.exit(1);
		}
		String sourceFileString=args[0];
		Renderer renderer=new Source(new File(sourceFileString)).getRenderer();
		renderer.setIncludeAlternateText(false);
		renderer.setIncludeHyperlinkURLs(false);
		// try-with-resources: Renderer.writeTo() does not close the Writer
		// it's given, and a BufferedWriter's last partial buffer is only
		// guaranteed to reach disk on flush/close - without this the process
		// could exit (or the caller could read the file) before the tail of
		// the output was actually written, silently truncating it.
		try (Writer writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(args[1]),"UTF-16LE"))) {
			renderer.writeTo(writer);
		}
  }
}
