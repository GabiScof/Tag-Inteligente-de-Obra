from flask import Flask, render_template, request, redirect, url_for, session, abort
from sqlalchemy.exc import IntegrityError
import os
import secrets
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
    descricao = db.Column(db.Text, nullable=False)        
    curiosidades = db.Column(db.Text, nullable=True)      
    imagem_url = db.Column(db.String(500), nullable=False)
    token = db.Column(db.String(50), nullable=True) 

    def __init__(self, titulo, artista, ano, descricao, curiosidades, imagem_url, token):
        self.titulo = titulo
        self.artista = artista
        self.ano = ano
        self.descricao = descricao
        self.curiosidades = curiosidades
        self.imagem_url = imagem_url
        self.token = token
        
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
        token = request.form.get("token")

        if not titulo or not token:
            return "Título obrigatório", 400

        try:
            # Corrigido para instanciar a classe Obra corretamente
            nova_obra = Obra(
                titulo=titulo,
                artista=artista,
                ano=ano,
                descricao=descricao,
                curiosidades=curiosidades,
                imagem_url=imagem_url,
                token = token
            )

            db.session.add(nova_obra)
            db.session.commit()
            print("Obra cadastrada com sucesso!")

        except Exception as e:
            db.session.rollback()
            print(f"Erro ao cadastrar obra: {e}")
            return "Erro ao salvar no banco de dados", 500

        return redirect("/admin")

    obras = Obra.query.order_by(Obra.id.desc()).all()

    print(f"Quantidade de obras encontradas: {len(obras)}")

    return render_template(
        "cadastro-obra.html",
        obras=obras
    )


@app.route("/visitantes/<int:visitante_id>/editar", methods=["POST"])
def editar_visitante(visitante_id):
    visitante = db.get_or_404(Visitante, visitante_id)
    nome = request.form.get("nome", "").strip()
    email = request.form.get("email", "").strip()
    rfid = request.form.get("rfid", "").strip()
    if not nome or not rfid or len(nome) > 100 or len(email) > 120 or len(rfid) > 50:
        return "Nome e RFID são obrigatórios e devem respeitar os limites de tamanho.", 400
    visitante.nome = nome
    visitante.email = email
    visitante.rfid = rfid
    try:
        db.session.commit()
    except IntegrityError:
        db.session.rollback()
        return "Este RFID já está cadastrado para outro visitante.", 409
    except Exception:
        db.session.rollback()
        app.logger.exception("Erro ao editar visitante")
        return "Erro ao atualizar visitante no banco de dados.", 500
    return redirect(url_for("listar_visitantes"))


@app.route("/visitantes/<int:visitante_id>/excluir", methods=["POST"])
def excluir_visitante(visitante_id):
    visitante = db.get_or_404(Visitante, visitante_id)
    try:
        db.session.delete(visitante)
        db.session.commit()
    except IntegrityError:
        db.session.rollback()
        return "Não foi possível excluir: existem registros vinculados a este visitante.", 409
    except Exception:
        db.session.rollback()
        app.logger.exception("Erro ao excluir visitante")
        return "Erro ao excluir visitante do banco de dados.", 500
    return redirect(url_for("listar_visitantes"))



@app.route("/obras/<int:obra_id>/editar", methods=["POST"])
def editar_obra(obra_id):
    obra = db.get_or_404(Obra, obra_id)

    titulo = (request.form.get("titulo") or "").strip()
    artista = (request.form.get("artista") or "").strip()
    ano = (request.form.get("ano") or "").strip()
    imagem_url = (request.form.get("imagem_url") or "").strip()
    descricao = (request.form.get("descricao") or "").strip()
    curiosidades = (request.form.get("curiosidades") or "").strip()
    token = (request.form.get("token") or "").strip()

    if not titulo:
        return "Título obrigatório", 400

    if (len(titulo) > 150 or len(artista) > 150
            or len(ano) > 50 or len(imagem_url) > 500):
        return "Um dos campos ultrapassou o limite permitido.", 400

    try:
        obra.titulo = titulo
        obra.artista = artista
        obra.ano = ano
        obra.imagem_url = imagem_url
        obra.descricao = descricao
        obra.curiosidades = curiosidades
        obra.token = token

        db.session.commit()

    except Exception:
        db.session.rollback()
        app.logger.exception("Erro ao editar obra")
        return "Erro ao atualizar obra no banco de dados.", 500

    return redirect(url_for("pagina_admin"))


@app.route("/obras/<int:obra_id>/excluir", methods=["POST"])
def excluir_obra(obra_id):
    obra = db.get_or_404(Obra, obra_id)

    try:
        db.session.delete(obra)
        db.session.commit()

    except IntegrityError:
        db.session.rollback()
        return "Não foi possível excluir: existem registros vinculados à obra.", 409

    except Exception:
        db.session.rollback()
        app.logger.exception("Erro ao excluir obra")
        return "Erro ao excluir obra do banco de dados.", 500

    return redirect(url_for("pagina_admin"))



@app.route("/obra/<string:token>")
def visualizar_obra(token):

    obra = (
        Obra.query
        .filter_by(token=token)
        .order_by(
            Obra.data_cadastro.desc().nullslast(),
            Obra.id.desc()
        )
        .first()
    )

    if obra is None:
        abort(404, description="Nenhuma obra encontrada para este token.")

    return render_template(
        "obra.html",
        obra=obra
    )

app.run(port=5000)

