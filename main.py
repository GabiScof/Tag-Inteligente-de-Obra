from flask import Flask, render_template, request, redirect
from flask_sqlalchemy import SQLAlchemy

app = Flask(__name__)

app.config['SQLALCHEMY_DATABASE_URI'] = 'postgresql://iot:pepcon-garton@postgresql.janks.dev.br:5432/projeto_b'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

class Visitante(db.Model):
    __tablename__ = 'visitante'
    
    id = db.Column(db.Integer, primary_key=True)
    nome = db.Column(db.String(100), nullable=False)
    email = db.Column(db.String(120), nullable=False)
    rfid = db.Column(db.String(50), unique=True, nullable=False)
    consentimento = db.Column(db.Boolean, nullable=False)

    def __init__(self, nome, email, rfid, consentimento):
        self.nome = nome
        self.email = email
        self.rfid = rfid
        self.consentimento = consentimento

class Obra(db.Model):
    __tablename__ = 'obra'
    
    id = db.Column(db.Integer, primary_key=True)
    titulo = db.Column(db.String(150), nullable=False)
    artista = db.Column(db.String(150), nullable=False)
    ano = db.Column(db.String(50), nullable=False)
    descricao = db.Column(db.Text, nullable=False)         # Alterado para Text (sem limite curto)
    curiosidades = db.Column(db.Text, nullable=True)      # Alterado para Text (e permitido vazio/null se necessário)
    imagem_url = db.Column(db.String(500), nullable=False) # Aumentado o limite para URLs compridas

    def __init__(self, titulo, artista, ano, descricao, curiosidades, imagem_url):
        self.titulo = titulo
        self.artista = artista
        self.ano = ano
        self.descricao = descricao
        self.curiosidades = curiosidades
        self.imagem_url = imagem_url
        
# Cria as tabelas no banco de dados automaticamente se não existirem
with app.app_context():
    db.create_all()

@app.route("/visitante", methods=["GET", "POST"])
def pagina_visitante():  # Nome da função alterado para evitar conflito
    if request.method == "POST":
        nome = request.form.get("nome")
        email = request.form.get("email")
        rfid = request.form.get("rfid")
        
        consentimento_str = request.form.get("consentimento")
        consentimento = True if consentimento_str in ["true", "on", "1"] else False

        if not nome or not rfid or not consentimento:
            return "Dados inválidos ou consentimento não aceito", 400

        try:
            novo_visitante = Visitante(
                nome=nome,
                email=email,
                rfid=rfid,
                consentimento=consentimento
            )

            db.session.add(novo_visitante)
            db.session.commit()
            print("Visitante cadastrado com sucesso!")

        except Exception as e:
            db.session.rollback()
            print(f"Erro ao cadastrar: {e}")
            return "Erro ao salvar no banco de dados", 500

        return redirect("/visitante")

    return render_template("cadastro-visitante.html")


@app.route("/visitantes")
def listar_visitantes():
    visitantes = Visitante.query.order_by(Visitante.id.desc()).all()

    return render_template(
        "lista-visitantes.html",
        visitantes=visitantes
    )

@app.route("/admin", methods=["GET", "POST"])
def pagina_admin():  # Nome da função único
    if request.method == "POST":
        titulo = request.form.get("titulo")
        artista = request.form.get("artista")
        ano = request.form.get("ano")
        descricao = request.form.get("descricao")
        curiosidades = request.form.get("curiosidades")
        imagem_url = request.form.get("imagem_url")

        if not titulo:
            return "Título obrigatório", 400

        try:
            # Corrigido para instanciar a classe Obra corretamente
            nova_obra = Obra(
                titulo=titulo,
                artista=artista,
                ano=ano,
                descricao=descricao,
                curiosidades=curiosidades,
                imagem_url=imagem_url
            )

            db.session.add(nova_obra)
            db.session.commit()
            print("Obra cadastrada com sucesso!")

        except Exception as e:
            db.session.rollback()
            print(f"Erro ao cadastrar obra: {e}")
            return "Erro ao salvar no banco de dados", 500

        return redirect("/admin")

    return render_template("cadastro-obra.html")

app.run(port=5000)