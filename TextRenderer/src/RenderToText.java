import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import net.htmlparser.jericho.Renderer;
import net.htmlparser.jericho.Source;

public class RenderToText {
	public static void main(String[] args) throws Exception {
		String sourceFileString=args[0];
		Renderer renderer=new Source(new File(sourceFileString)).getRenderer();
		renderer.setIncludeAlternateText(false);
		renderer.setIncludeHyperlinkURLs(false);
		renderer.writeTo(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(args[1]),"UTF-16LE")));
  }
}
